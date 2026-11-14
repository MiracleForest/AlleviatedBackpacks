// The nerfs (`alleviatedbackpacks-server.json` -> nerfs,
// Config.NerfsConfig / CommonEventHandler.onWorldTick):
//
// - tooManyBackpacksSlowness (default off): every 40 ticks per dimension, a player carrying more
//   than `maxNumberOfBackpacks` backpacks (inventory, offhand, chest slot) gets slowness for 80
//   ticks, ceil((count - max) * slownessLevelsPerAdditionalBackpack) levels (at most 10), without
//   particles. The effect is always slowness.
// - onlyWornBackpackTriggersUpgrades (default off): only the backpack in the chest slot runs its
//   active upgrades (ticks, pickup, ...), through PlayerScan's carried upgrade gate.

#include "feature/nerf/Nerf.h"

#include <map>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"

#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/effect/EffectDuration.h"
#include "mc/world/effect/MobEffect.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/level/Level.h"

#include "core/WorldRules.h"
#include "feature/admin/AbpConfig.h"
#include "game/PlayerScan.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace abp::nerf {

namespace {
constexpr int kCheckCooldownTicks = 40; ///< CommonEventHandler: BACKPACK_CHECK_COOLDOWN
} // namespace

MobEffect const* slownessEffect() { return MobEffect::getByName("slowness"); }

int applySlowness(Player& player) {
    auto const& nerfs = admin::abpServer().nerfs;
    if (!nerfs.tooManyBackpacksSlowness) return 0;
    int count  = static_cast<int>(game::scanBackpacks(player).size());
    int levels = core::slownessLevels(
        count,
        std::clamp(nerfs.maxNumberOfBackpacks, 1, 27),
        std::clamp(nerfs.slownessLevelsPerAdditionalBackpack, 0.1, 5.0)
    );
    if (levels <= 0) return 0;
    auto const* effect = slownessEffect();
    if (!effect) return 0;
    MobEffectInstance instance(effect->mId, EffectDuration{kCheckCooldownTicks * 2});
    instance.mAmplifier     = levels - 1;
    instance.mAmbient       = false;
    instance.mEffectVisible = false;
    player.addEffect(instance);
    return levels;
}

bool upgradesActive(game::CarriedBackpack const& carried) {
    if (!admin::abpServer().nerfs.onlyWornBackpackTriggersUpgrades) return true;
    // Nested backpacks belong to their outermost carried backpack: worn means that one is worn.
    return carried.where.kind == game::ItemLocation::Kind::Armor
        && carried.where.slot == static_cast<int>(SharedTypes::Legacy::ArmorSlot::Torso);
}

namespace {

class NerfModule {
public:
    void install() {
        game::setCarriedUpgradeGate([](Player&, game::CarriedBackpack const& c) { return upgradesActive(c); });
        mListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(
            [this](ll::event::ServerLevelTickEvent& ev) { tick(ev.level()); }
        );
    }
    void uninstall() {
        ll::event::EventBus::getInstance().removeListener(mListener);
        game::setCarriedUpgradeGate(nullptr);
        mNextCheck.clear();
    }

private:
    void tick(Level& level) {
        ++mNow;
        if (!admin::abpServer().nerfs.tooManyBackpacksSlowness) return;
        std::map<int, std::vector<Player*>> byDim;
        level.forEachPlayer([&](Player& p) {
            if (!p.isClientSide() && p.isAlive()) byDim[static_cast<int>(p.getDimensionId())].push_back(&p);
            return true;
        });
        for (auto& [dim, players] : byDim) {
            auto& next = mNextCheck[dim];
            if (next > mNow) continue;
            next = mNow + kCheckCooldownTicks;
            for (auto* p : players) applySlowness(*p);
        }
    }

    ll::event::ListenerPtr       mListener;
    std::int64_t                 mNow = 0;
    std::map<int, std::int64_t> mNextCheck;
};

} // namespace
} // namespace abp::nerf

ABP_REGISTER_MODULE(abp::nerf::NerfModule);
