// Selftests of the Tank upgrade and the inventory columns it takes: tank.*
// Run with `/backpack selftest "tank.*"`.

#include "mc/world/PlayerUIContainer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/PlayerUISlot.h"
#include "mc/world/item/ItemStack.h"

#include "api/BackpackCapabilities.h"
#include "core/Capacity.h"
#include "game/Backpack.h"
#include "game/ItemConv.h"
#include "net/NativeScreens.h"
#include "selftest/SelfTest.h"
#include "upgrade/UpgradeManager.h"
#include "upgrade/tank/Columns.h"
#include "upgrade/tank/MenuBits.h"
#include "upgrade/tank/Tank.h"

namespace abp::upgrade::tank {
namespace {

using selftest::Context;
namespace fl = core::fluid;

core::ItemStackData itemOf(std::string_view id, int count = 1, int aux = 0) {
    return game::ItemConv::toData(game::ItemConv::make(id, count, aux));
}

ABP_REGISTER_SELFTEST("tank.capacity", [](Context& ctx) {
    auto leather = ctx.createBackpack(core::Tier::Leather);
    ctx.installUpgrade(leather, "tank");
    ctx.check(capacity(*leather) == 12000, "leather: 3 rows x 4000 = 12000, got " + std::to_string(capacity(*leather)));
    ctx.check(maxInOut(*leather) == 1000, "leather: max(1000, 20 x 3) per operation");
    auto diamond = ctx.createBackpack(core::Tier::Diamond);
    ctx.installUpgrade(diamond, "tank");
    ctx.check(capacity(*diamond) == 36000, "diamond: 9 rows of 12 x 4000 = 36000, got " + std::to_string(capacity(*diamond)));
    auto iron = ctx.createBackpack(core::Tier::Iron);
    ctx.installUpgrade(iron, "tank");
    ctx.installUpgrade(iron, "tank");
    ctx.check(capacity(*iron) == 24000, "iron keeps its 6 base rows with two tanks: 24000, got " + std::to_string(capacity(*iron)));
});

ABP_REGISTER_SELFTEST("tank.columns", [](Context& ctx) {
    auto bp = ctx.createBackpack(core::Tier::Iron); // 54 slots, 6 rows
    auto& inv = bp->inventory();
    inv.set(20, itemOf("minecraft:diamond", 5));
    inv.set(0, itemOf("minecraft:stone", 10));
    int a = ctx.installUpgrade(bp, "tank");
    ctx.check(inv.size() == 42 && bp->data().columnsTaken == 2, "a tank takes 2 columns: 42 slots, got " + std::to_string(inv.size()));
    ctx.check(inv.at(0).id == "minecraft:stone", "slot 0 keeps its item");
    ctx.check(inv.at(20).id == "minecraft:diamond" && inv.at(20).count == 5, "items never move");
    ctx.installUpgrade(bp, "tank");
    ctx.check(inv.size() == 30 && bp->data().columnsTaken == 4, "two tanks: 30 slots");
    auto removed = upgrade::remove(bp, a, nullptr);
    ctx.require(removed.has_value(), "remove a tank");
    ctx.check(inv.size() == 42 && bp->data().columnsTaken == 2, "removing gives the columns back");

    // One item in a slot that would go away blocks the tank, even with free slots in front.
    auto tail = ctx.createBackpack(core::Tier::Iron);
    tail->inventory().set(53, itemOf("minecraft:diamond", 5));
    std::string tailReason;
    ctx.check(!upgrade::canInstall(tail, 0, itemOf(kItemId), nullptr, &tailReason), "an item in the last columns refuses a tank");
    ctx.check(tail->inventory().size() == 54 && tail->inventory().at(53).count == 5, "inventory unchanged");

    // A full inventory can't give columns either: nothing changes.
    auto full = ctx.createBackpack(core::Tier::Leather);
    for (int i = 0; i < full->inventory().size(); ++i) full->inventory().set(i, itemOf("minecraft:dirt", 1));
    std::string reason;
    ctx.check(!upgrade::canInstall(full, 0, itemOf(kItemId), nullptr, &reason), "full backpack refuses a tank");
    ctx.check(reason.find("last 6 slots") != std::string::npos, "message names the 6 slots: " + reason);
    ctx.check(full->inventory().size() == 27, "inventory unchanged");
    // Persisted: the reduced inventory survives a reload.
    auto back = core::BackpackData::fromJson(bp->data().toJson());
    ctx.check(back && back->inventory.size() == 42 && back->columnsTaken == 2, "columns survive save/load");
});

ABP_REGISTER_SELFTEST("tank.install_limits", [](Context& ctx) {
    auto bp = ctx.createBackpack(core::Tier::Diamond);
    ctx.installUpgrade(bp, "tank");
    ctx.installUpgrade(bp, "tank");
    std::string reason;
    ctx.check(!upgrade::canInstall(bp, 2, itemOf(kItemId), nullptr, &reason), "a third tank is refused");
    ctx.check(reason.find("already two Tank Upgrades") != std::string::npos, "message: " + reason);

    // A tank holding more than a small backpack's capacity can't go into it.
    UpgradeContext t(bp, 0);
    write(t, fl::Tank({"water", 30000}, capacity(*bp)));
    auto item = upgrade::remove(bp, 0, nullptr);
    ctx.require(item.has_value(), "remove the filled tank");
    auto small = ctx.createBackpack(core::Tier::Leather); // 12000 mB
    reason.clear();
    ctx.check(!upgrade::canInstall(small, 0, *item, nullptr, &reason), "too small for 30000 mB");
    ctx.check(reason.find("at least 2.5x higher capacity") != std::string::npos, "message with 2.5x: " + reason);
    auto big = ctx.createBackpack(core::Tier::Netherite);
    ctx.require(upgrade::install(big, 0, *item, nullptr, &reason), "fits a netherite backpack: " + reason);
    ctx.check(read(UpgradeContext(big, 0)).contents() == FluidStack{"water", 30000}, "contents travel with the item");
});

ABP_REGISTER_SELFTEST("tank.max_in_out", [](Context& ctx) {
    auto           bp = ctx.createBackpack(core::Tier::Leather);
    UpgradeContext t(bp, ctx.installUpgrade(bp, "tank"));
    ctx.check(fillTank(t, {"minecraft:water", 5000}, false) == 1000, "one operation moves a bucket");
    ctx.check(fillTank(t, {"minecraft:water", 5000}, false, true) == 5000, "ignoring the limit moves all");
    ctx.check(fillTank(t, {"minecraft:lava", 1000}, true) == 0, "another fluid is refused");
    ctx.check(drainTank(t, 9000, false).amount == 1000, "drain: a bucket per operation");
    auto h = api::backpack::fluidHandler(bp);
    ctx.require(h != nullptr, "backpack fluid handler");
    ctx.check(h->fill({"minecraft:water", 100000}, false) == 1000, "the API handler is limited too");
    ctx.check(read(t).amount() == 6000, "6000 mB in the tank, got " + std::to_string(read(t).amount()));
});

ABP_REGISTER_SELFTEST("tank.slots", [](Context& ctx) {
    auto           bp = ctx.createBackpack(core::Tier::Leather);
    UpgradeContext t(bp, ctx.installUpgrade(bp, "tank"));
    // Input: a water bucket is drained; the empty bucket stays in the input slot.
    fluidui::setSlotItem(t, "in", itemOf("minecraft:water_bucket"));
    ctx.check(processSlots(t), "input processed");
    ctx.check(read(t).contents() == FluidStack{"water", 1000}, "1000 mB water");
    ctx.check(fluidui::slotItem(t, "in").id == "minecraft:bucket", "the empty bucket stays in the input slot");
    ctx.check(!processSlots(t), "an empty bucket in the input slot does nothing");
    // Output: a glass bottle is filled in place.
    fluidui::setSlotItem(t, "out", itemOf("minecraft:glass_bottle"));
    ctx.check(processSlots(t), "output processed");
    ctx.check(read(t).amount() == 750, "250 mB went into the bottle");
    auto out = fluidui::slotItem(t, "out");
    ctx.check(out.id == "minecraft:potion" && out.aux == 0 && out.count == 1, "a water bottle in the output slot");
    // A bottle o' enchanting is no fluid container (the mod has no handler for it): nothing moves.
    auto           bp2 = ctx.createBackpack(core::Tier::Leather);
    UpgradeContext x(bp2, ctx.installUpgrade(bp2, "tank"));
    fluidui::setSlotItem(x, "in", itemOf("minecraft:experience_bottle"));
    (void)processSlots(x);
    ctx.check(read(x).empty() && fluidui::slotItem(x, "in").count == 1, "bottle o' enchanting left alone");
    // Items that are no fluid containers are refused by the slot.
    fluidui::SettingsItemSlot slot(t, "in", nullptr, 1, nullptr);
    ctx.check(slot.accept(ctx.player(), game::ItemConv::make("minecraft:stone")) == 0, "stone refused");
});

ABP_REGISTER_SELFTEST("tank.legacy_slots", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    auto  bp     = ctx.createBackpack(core::Tier::Leather);
    UpgradeContext t(bp, ctx.installUpgrade(bp, "tank"));
    ctx.require(ctx.give(player, ctx.backpackItem(*bp), 9) == 9, "carry the backpack");
    // Data from an older layout: result slots and several containers per slot.
    fluidui::setSlotItem(t, "inResult", itemOf("minecraft:bucket", 2));
    fluidui::setSlotItem(t, "out", itemOf("minecraft:glass_bottle", 3));
    co_await ctx.ticks(15); // upgrades tick every 10 ticks (upgradeTickInterval)
    ctx.check(fluidui::slotItem(t, "inResult").empty(), "the result slot was emptied");
    ctx.check(fluidui::slotItem(t, "out").count == 1, "one container left in the output slot");
    long long buckets = bp->inventory().count([](core::ItemStackData const& d) { return d.id == "minecraft:bucket"; });
    long long bottles = bp->inventory().count([](core::ItemStackData const& d) { return d.id == "minecraft:glass_bottle"; });
    ctx.check(buckets == 2 && bottles == 2, "the extra items went into the backpack");
});

ABP_REGISTER_SELFTEST("tank.cooldown", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    auto  bp     = ctx.createBackpack(core::Tier::Leather);
    UpgradeContext t(bp, ctx.installUpgrade(bp, "tank"));
    ctx.require(ctx.give(player, ctx.backpackItem(*bp), 9) == 9, "carry the backpack");
    fluidui::setSlotItem(t, "out", itemOf("minecraft:bucket"));
    write(t, fl::Tank({"water", 3000}, capacity(*bp)));
    co_await ctx.ticks(15); // the first step comes with the first upgrade tick (every 10 ticks)
    ctx.check(fluidui::slotItem(t, "out").id == "minecraft:water_bucket" && read(t).amount() == 2000, "the bucket was filled");
    // A new empty bucket waits for the 20 tick cooldown.
    fluidui::setSlotItem(t, "out", itemOf("minecraft:bucket"));
    co_await ctx.ticks(3);
    ctx.check(fluidui::slotItem(t, "out").id == "minecraft:bucket" && read(t).amount() == 2000, "nothing during the cooldown");
    co_await ctx.ticks(20);
    ctx.check(fluidui::slotItem(t, "out").id == "minecraft:water_bucket" && read(t).amount() == 1000, "filled after the cooldown");
    // A full bucket stays in the slot and is not filled again.
    co_await ctx.ticks(25);
    ctx.check(read(t).amount() == 1000, "a full bucket takes nothing more");
});

ABP_REGISTER_SELFTEST("tank.cursor_click", [](Context& ctx) {
    auto& player = ctx.player();
    auto  bp     = ctx.createBackpack(core::Tier::Leather);
    UpgradeContext t(bp, ctx.installUpgrade(bp, "tank"), &player);
    auto r = interactWithCursor(t, game::ItemConv::make("minecraft:lava_bucket"));
    ctx.check(r && game::ItemConv::idOf(*r) == "minecraft:bucket", "a lava bucket on the cursor is emptied");
    ctx.check(read(t).contents() == FluidStack{"lava", 1000}, "1000 mB lava");
    r = interactWithCursor(t, game::ItemConv::make("minecraft:bucket"));
    ctx.check(r && game::ItemConv::idOf(*r) == "minecraft:lava_bucket", "an empty bucket is filled");
    ctx.check(read(t).empty(), "tank empty again");
    ctx.check(!interactWithCursor(t, game::ItemConv::make("minecraft:stone")), "stone does nothing");
    // Item fluid handlers only work on a single item.
    ctx.check(!interactWithCursor(t, game::ItemConv::make("minecraft:water_bucket", 2)), "count > 1 does nothing");

    // Through the native screen's tab action (TankClickPayload).
    player.setPlayerUIItem(PlayerUISlot::CursorSelected, game::ItemConv::make("minecraft:water_bucket"), false);
    bool handled = false;
    for (auto const& h : net::TabActionRegistry::entries()) {
        if (h.upgrade == "tank" && h.handle) handled |= h.handle(player, t, nlohmann::json{{"op", "click"}});
    }
    ctx.check(handled, "the tab action is handled");
    ctx.check(game::ItemConv::idOf(fluidui::cursorOf(player)) == "minecraft:bucket", "the cursor got the empty bucket");
    ctx.check(read(t).contents() == FluidStack{"water", 1000}, "the water is in the tank");
    player.setPlayerUIItem(PlayerUISlot::CursorSelected, ItemStack{}, false);
});

ABP_REGISTER_SELFTEST("tank.render_info", [](Context& ctx) {
    auto bp = ctx.createBackpack(core::Tier::Iron); // 2 upgrade slots: slot 1 is the right half
    UpgradeContext t(bp, ctx.installUpgrade(bp, "tank", 1));
    write(t, fl::Tank({"water", 12000}, capacity(*bp)));
    auto info = net::buildRenderInfo(*bp);
    ctx.check(info.extra.contains("tanks") && info.extra["tanks"].contains("right"), "a single tank in the right half renders right");
    ctx.check(info.extra["tanks"]["right"].value("fillRatio", 0.f) == 0.5f, "fill ratio 0.5");
    ctx.installUpgrade(bp, "tank", 0);
    info = net::buildRenderInfo(*bp);
    ctx.check(info.extra["tanks"].contains("left") && info.extra["tanks"].contains("right"), "two tanks: left and right");
});

ABP_REGISTER_SELFTEST("tank.tab_data", [](Context& ctx) {
    auto& player = ctx.player();
    auto  bp     = ctx.createBackpack(core::Tier::Leather);
    UpgradeContext t(bp, ctx.installUpgrade(bp, "tank"), &player);
    write(t, fl::Tank({"experience", 3200}, capacity(*bp)));
    nlohmann::json tab = nlohmann::json::object();
    for (auto const& p : net::TabDataRegistry::entries()) {
        if (p.upgrade == "tank" && p.data) tab.update(p.data(player, t));
    }
    ctx.check(tab["fluidTank"]["capacity"] == 12000, "capacity sent");
    ctx.check(tab["fluidTank"]["fluid"]["amount"] == 3200 && tab["fluidTank"]["fluid"]["xp"] == true, "experience flagged");
    ctx.check(tab["columnsTaken"] == 2 && tab["inventoryColumns"] == 2, "columns sent");
});

} // namespace
} // namespace abp::upgrade::tank
