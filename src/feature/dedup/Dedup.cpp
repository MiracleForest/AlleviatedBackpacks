#include "feature/dedup/Dedup.h"

#include <map>
#include <optional>
#include <string>
#include <unordered_set>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/item/ItemActor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/Spawner.h"
#include "mc/world/phys/AABB.h"

#include "feature/admin/AbpConfig.h"
#include "game/BackpackItem.h"
#include "game/PlayerScan.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "ui/menu/BackpackMenu.h"
#include "ui/menu/MenuManager.h"

namespace abp::dedup {

namespace {

constexpr int   kCheckCooldownTicks = 40; ///< CommonEventHandler: BACKPACK_CHECK_COOLDOWN
constexpr float kAreaInflate        = 10.0f;

/// A backpack item with a uuid.
std::optional<std::string> dedupeKey(ItemStack const& stack) {
    if (stack.isNull() || !game::isBackpack(stack)) return std::nullopt;
    return game::backpackUuid(stack);
}

} // namespace

void removeUuid(ItemStack& stack) {
    if (stack.isNull() || !stack.mUserData) return;
    auto tag = stack.mUserData->clone();
    tag->remove(game::kUuidKey);
    stack.setUserData(std::move(tag));
}

int dedupeItemEntityInArea(ItemActor& entity, BlockSource& region) {
    auto uuid = dedupeKey(entity.item());
    if (!uuid) return 0;
    auto const& b = entity.getAABB();
    AABB        area{
        Vec3{b.min.x - kAreaInflate, b.min.y - kAreaInflate, b.min.z - kAreaInflate},
        Vec3{b.max.x + kAreaInflate, b.max.y + kAreaInflate, b.max.z + kAreaInflate}
    };
    for (auto* actor : region.getEntities(area, 0.0f, ActorType::ItemEntity)) {
        if (!actor || actor == &entity || !actor->isType(ActorType::ItemEntity) || !actor->isAlive()) continue;
        if (dedupeKey(static_cast<ItemActor&>(*actor).item()) != uuid) continue;
        // One match is enough: the item entity that just appeared gives up the uuid.
        removeUuid(entity.item());
        log().info("dedup: a dropped backpack item lost uuid {} (another drop nearby has it)", *uuid);
        return 1;
    }
    return 0;
}

int dedupePlayers(int dimension) {
    auto level = ll::service::getLevel();
    if (!level) return 0;
    std::unordered_set<std::string> seen;
    int                             removed = 0;
    level->forEachPlayer([&](Player& player) {
        if (player.isClientSide() || static_cast<int>(player.getDimensionId()) != dimension) return true;
        for (auto const& c : game::scanBackpacks(player)) {
            ItemStack stack = game::itemAt(player, c.where);
            auto      uuid  = dedupeKey(stack);
            if (!uuid) continue;
            if (seen.insert(*uuid).second) continue; // first one seen keeps it
            removeUuid(stack);
            game::setItemAt(player, c.where, stack);
            ++removed;
        }
        return true;
    });
    if (removed > 0) log().info("dedup: {} duplicate backpack(s) in player inventories lost their uuid", removed);
    return removed;
}

int dedupeOnOpen(Player& player, game::ItemLocation openedAt) {
    auto uuid = dedupeKey(game::itemAt(player, openedAt));
    if (!uuid) return 0;
    int removed = 0;
    for (auto const& c : game::scanBackpacks(player)) {
        if (c.where == openedAt || dedupeKey(game::itemAt(player, c.where)) != uuid) continue;
        ItemStack other = game::itemAt(player, c.where);
        removeUuid(other);
        game::setItemAt(player, c.where, other);
        ++removed;
    }
    if (removed > 0) log().info("dedup: {} duplicate(s) of the opened backpack {} lost their uuid", removed, *uuid);
    return removed;
}

namespace {

LL_TYPE_INSTANCE_HOOK(
    DedupSpawnItemHook,
    ll::memory::HookPriority::Normal,
    Spawner,
    &Spawner::$spawnItem,
    ItemActor*,
    BlockSource&     region,
    ItemStack const& inst,
    Actor*           spawner,
    Vec3 const&      pos,
    int              throwTime
) {
    auto* actor = origin(region, inst, spawner, pos, throwTime);
    if (actor && !region.getLevel().isClientSide() && game::isBackpack(inst)) {
        try {
            dedupeItemEntityInArea(*actor, region);
        } catch (std::exception const& e) {
            log().error("dedup: {}", e.what());
        }
    }
    return actor;
}

class DedupModule {
public:
    void install() {
        DedupSpawnItemHook::hook();
        mListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(
            [this](ll::event::ServerLevelTickEvent& ev) { tick(ev.level()); }
        );
        ui::MenuManager::Observer o;
        // BackpackContainer: opening a backpack clears the uuid of the player's copies.
        o.beforeShow = [](Player& player, ui::Menu& menu, int) {
            auto* bm = dynamic_cast<ui::BackpackMenu*>(&menu);
            if (!bm || !bm->source() || player.isClientSide()) return;
            dedupeOnOpen(player, *bm->source());
        };
        mObserver = ui::MenuManager::get().addObserver(std::move(o));
    }
    void uninstall() {
        ll::event::EventBus::getInstance().removeListener(mListener);
        ui::MenuManager::get().removeObserver(mObserver);
        DedupSpawnItemHook::unhook();
        mNextCheck.clear();
    }

private:
    void tick(Level& level) {
        ++mNow;
        if (admin::abpServer().tickDedupeLogicDisabled) return;
        std::map<int, bool> dims;
        level.forEachPlayer([&](Player& p) {
            if (!p.isClientSide()) dims[static_cast<int>(p.getDimensionId())] = true;
            return true;
        });
        for (auto const& [dim, _] : dims) {
            auto& next = mNextCheck[dim];
            if (next > mNow) continue;
            next = mNow + kCheckCooldownTicks;
            dedupePlayers(dim);
        }
    }

    ll::event::ListenerPtr  mListener;
    int                     mObserver = 0;
    std::int64_t            mNow      = 0;
    std::map<int, std::int64_t> mNextCheck;
};

} // namespace
} // namespace abp::dedup

ABP_REGISTER_MODULE(abp::dedup::DedupModule);
