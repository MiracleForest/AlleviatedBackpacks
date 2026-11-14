// Showcase e2e support (`tools/e2e/client/scenarios/showcase.ps1`):
//
//   /backpack showcase backpacks   gives two filled backpacks into the first free inventory slots:
//                                  a diamond backpack with a player-like inventory and five
//                                  upgrades, then a gold backpack with a few stacks and two upgrades.
//                                  Logs "showcase backpacks: <uuid> <uuid>".

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ll/api/command/CommandHandle.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"

#include "core/BackpackInventory.h"
#include "core/Tier.h"
#include "game/Backpack.h"
#include "game/BackpackItem.h"
#include "game/ItemConv.h"
#include "game/PlayerScan.h"
#include "game/Storage.h"
#include "mod/Commands.h"
#include "mod/Log.h"
#include "upgrade/IUpgrade.h"
#include "upgrade/UpgradeManager.h"

namespace {
using namespace abp;

struct Fill {
    std::string_view id;
    int              count;
};

std::shared_ptr<game::Backpack>
makeFilled(Player& player, core::Tier tier, std::vector<Fill> const& items, std::vector<std::string_view> const& upgrades) {
    auto bp = game::Storage::get().create(tier);
    if (!bp) return nullptr;
    core::AccessContext ctx{core::Source::Internal, {}};
    for (auto const& [id, count] : items) {
        auto stack = game::ItemConv::make("minecraft:" + std::string(id), 1);
        if (stack.isNull()) continue;
        (void)bp->inventory().insert(game::ItemConv::toData(stack).withCount(count), ctx);
    }
    int slot = 0;
    for (auto id : upgrades) {
        auto const* u = upgrade::byId(id);
        if (!u || slot >= static_cast<int>(bp->data().upgrades.size())) continue;
        if (upgrade::install(bp, slot, game::ItemConv::toData(game::ItemConv::make(u->info().itemId, 1)), &player)) ++slot;
    }
    bp->markDirty();
    game::Storage::get().save(*bp, true);
    return bp;
}

void giveToFreeSlot(Player& player, ItemStack const& item) {
    for (int i = 0; i < 36; ++i) {
        auto loc = game::ItemLocation{game::ItemLocation::Kind::Inventory, i};
        if (game::itemAt(player, loc).isNull()) {
            game::setItemAt(player, loc, item);
            return;
        }
    }
}
} // namespace

ABP_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload().text("showcase").text("backpacks").execute([](CommandOrigin const& origin, CommandOutput& output) {
        if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors) {
            output.error("operators only");
            return;
        }
        auto* e = origin.getEntity();
        if (!e || !e->isPlayer() || !game::Storage::get().available()) {
            output.error("players only");
            return;
        }
        auto& player = *static_cast<Player*>(e);
        std::vector<Fill> showItems{
            {"oak_log", 192},         {"birch_log", 128},     {"oak_planks", 256},    {"cobblestone", 768},
            {"cobbled_deepslate", 128}, {"stone_bricks", 128}, {"bricks", 64},        {"dirt", 256},
            {"gravel", 64},           {"sand", 192},          {"glass", 64},          {"torch", 192},
            {"coal", 160},            {"raw_iron", 48},       {"iron_ingot", 64},     {"raw_gold", 30},
            {"gold_ingot", 41},       {"copper_ingot", 64},   {"redstone", 192},      {"lapis_lazuli", 64},
            {"diamond", 23},          {"emerald", 17},        {"amethyst_shard", 24}, {"bread", 32},
            {"apple", 16},            {"cooked_beef", 24},    {"carrot", 40},         {"potato", 40},
            {"golden_carrot", 12},    {"melon_slice", 48},    {"cooked_porkchop", 20}, {"baked_potato", 30},
            {"pumpkin_pie", 8},       {"cake", 1},            {"clay_ball", 32},      {"slime_ball", 12},
            {"name_tag", 2},          {"saddle", 1},          {"compass", 1},         {"clock", 1},
            {"book", 6},              {"experience_bottle", 16}, {"shears", 1},       {"fishing_rod", 1},
            {"bucket", 1},            {"iron_pickaxe", 1},    {"diamond_sword", 1},   {"bow", 1},
            {"arrow", 64},            {"string", 20},         {"leather", 12},        {"feather", 16},
            {"bone", 18},             {"ender_pearl", 8},     {"flint", 12},          {"wheat_seeds", 30},
            {"sugar_cane", 30},       {"oak_sapling", 12},    {"poppy", 6},           {"dandelion", 8},
        };
        auto show = makeFilled(player, core::Tier::Diamond, showItems, {"crafting", "magnet", "feeding", "pickup", "smelting"});
        auto tip  = makeFilled(
            player,
            core::Tier::Gold,
            {{"cooked_beef", 30}, {"iron_ingot", 64}, {"oak_log", 128}, {"torch", 40}, {"diamond", 9}},
            {"feeding", "magnet"}
        );
        if (!show || !tip) {
            output.error("could not create the backpacks");
            return;
        }
        giveToFreeSlot(player, game::makeBackpackItem(core::Tier::Diamond, show->uuid()));
        giveToFreeSlot(player, game::makeBackpackItem(core::Tier::Gold, tip->uuid()));
        player.sendInventory(false);
        log().info("showcase backpacks: {} {}", show->uuid(), tip->uuid());
        output.success("showcase backpacks: " + show->uuid() + " " + tip->uuid());
    });
});
