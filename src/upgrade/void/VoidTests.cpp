// Void upgrade selftests: `/backpack selftest "void.*"`.

#include <memory>

#include "mc/world/PlayerUIContainer.h"
#include "mc/world/actor/player/PlayerUISlot.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"

#include "api/BackpackCapabilities.h"
#include "api/abp/Fluid.h"
#include "game/ItemConv.h"
#include "mod/Config.h"
#include "net/NativeScreens.h"
#include "selftest/SelfTest.h"
#include "ui/menu/MenuManager.h"
#include "upgrade/UpgradeManager.h"
#include "upgrade/void/VoidUpgrade.h"

namespace abp::upgrade::voiding {
namespace {

using core::Tier;
using selftest::Context;
using Where  = ui::MenuManager::SlotAddress::Where;
using Action = ui::MenuManager::ActionType;

core::AccessContext const kPickup{core::Source::Pickup};
core::AccessContext const kAutomation{core::Source::Automation};
core::AccessContext const kPlayer{core::Source::Player};
core::AccessContext const kInternal{core::Source::Internal};

core::ItemStackData data(std::string_view id, int count) { return game::ItemConv::toData(game::ItemConv::make(id, count)); }

int countOf(game::Backpack& bp, std::string_view id) {
    return static_cast<int>(bp.inventory().count([&](core::ItemStackData const& s) { return s.id == id; }));
}

int slotsWith(game::Backpack& bp, std::string_view id) {
    int n = 0;
    for (int i = 0; i < bp.inventory().size(); ++i) n += bp.inventory().at(i).id == id ? 1 : 0;
    return n;
}

/// Installs `id` and configures it: filter mode + ghost items, overflow mode, work in GUI, and
/// optionally exact (item + NBT + durability) matching.
UpgradeContext setup(
    Context&                               ctx,
    std::shared_ptr<game::Backpack> const& bp,
    std::string const&                     id,
    std::vector<std::string_view> const&   ghosts,
    core::FilterMode                       mode      = core::FilterMode::Allow,
    bool                                   overflow  = false,
    bool                                   workInGui = false,
    bool                                   exact     = false
) {
    int            slot = ctx.installUpgrade(bp, id);
    UpgradeContext uc(bp, slot);
    core::Filter   f(tierConfig(id == kAdvancedId).filterSlots);
    f.mode = mode;
    if (exact) {
        f.matchBy         = core::MatchBy::Item;
        f.matchNbt        = true;
        f.matchDurability = true;
    }
    for (int i = 0; i < static_cast<int>(ghosts.size()); ++i) f.setGhost(i, data(ghosts[static_cast<std::size_t>(i)], 1));
    uc.settings()[std::string(kFilterKey)]       = f.toJson();
    uc.settings()[std::string(kVoidOverflowKey)] = overflow;
    uc.settings()[std::string(kWorkInGuiKey)]    = workInGui;
    uc.saveSettings(true);
    return uc;
}

/// The native tab data / actions registered for `group` (what the client tab gets and sends).
nlohmann::json tabData(Player& p, UpgradeContext const& ctx, std::string_view group) {
    auto out = nlohmann::json::object();
    for (auto const& e : net::TabDataRegistry::entries()) {
        if (e.upgrade == group && e.data) out.update(e.data(p, ctx));
    }
    return out;
}
bool tabAction(Player& p, UpgradeContext const& ctx, std::string_view group, nlohmann::json const& data) {
    for (auto const& h : net::TabActionRegistry::entries()) {
        if (h.upgrade == group && h.handle && h.handle(p, ctx, data)) return true;
    }
    return false;
}

ABP_REGISTER_SELFTEST("void.defaults_and_config", [](Context& ctx) {
    auto const& c = config::get<VoidConfig>();
    ctx.check(c.voidUpgrade.filterSlots == 9 && c.voidUpgrade.slotsInRow == 3 && c.voidUpgrade.voidAnythingEnabled, "basic config defaults");
    ctx.check(c.advancedVoidUpgrade.filterSlots == 16 && c.advancedVoidUpgrade.slotsInRow == 4, "advanced config defaults");
    auto bp   = ctx.createBackpack(Tier::Iron);
    int  slot = ctx.installUpgrade(bp, "void");
    auto s    = VoidSettings::read(UpgradeContext(bp, slot));
    ctx.check(s.filter.mode == core::FilterMode::Allow && !s.shouldVoidOverflow() && !s.workInGui, "defaults: allow list, Void Any, not in GUI");
    auto r = bp->inventory().insert(data("minecraft:stone", 10), kPickup);
    ctx.check(r.inserted == 10 && r.voided == 0, "empty allow list voids nothing");
    ctx.check(canInstall(bp, 1, data("alleviatedbackpacks:advanced_void_upgrade", 1), nullptr), "no void limit");

    auto deny = ctx.createBackpack(Tier::Leather);
    setup(ctx, deny, "void", {}, core::FilterMode::Deny);
    r = deny->inventory().insert(data("minecraft:stone", 10), kPickup);
    ctx.check(r.voided == 10, "an empty block list voids everything");

    // Settings of earlier builds: a mode name other than "always" means overflow only.
    UpgradeContext uc(bp, slot);
    uc.settings()["voidType"] = "slot_overflow";
    ctx.check(VoidSettings::read(uc).shouldVoidOverflow(), "legacy mode name read as Void Overflow");
    uc.settings()["voidType"] = "always";
    ctx.check(!VoidSettings::read(uc).shouldVoidOverflow(), "legacy always read as Void Any");
    uc.settings().erase("voidType");
});

ABP_REGISTER_SELFTEST("void.void_any", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    auto  bp     = ctx.createBackpack(Tier::Iron); // 2 upgrade slots
    setup(ctx, bp, "void", {"minecraft:cobblestone"});
    auto r = bp->inventory().insert(data("minecraft:cobblestone", 5), kAutomation);
    ctx.check(r.voided == 5 && r.inserted == 0, "automation insert voided");
    r = bp->inventory().insert(data("minecraft:cobblestone", 5), kPickup);
    ctx.check(r.voided == 5, "pickup insert voided");
    r = bp->inventory().insert(data("minecraft:cobblestone", 5), kInternal);
    ctx.check(r.inserted == 5 && r.voided == 0, "internal moves are never voided");
    r = bp->inventory().insert(data("minecraft:dirt", 7), kPickup);
    ctx.check(r.inserted == 7, "non-matching item stored");
    r = bp->inventory().insert(data("minecraft:cobblestone", 4), kPlayer);
    ctx.check(r.inserted == 4 && r.voided == 0, "GUI insert kept (work in GUI off)");

    // Without a Pickup upgrade nothing is picked up into the backpack, so nothing is voided.
    ctx.require(ctx.give(player, ctx.backpackItem(*bp), 3) == 3, "give backpack");
    co_await ctx.ticks(2);
    auto cobble = data("minecraft:cobblestone", 20);
    ctx.check(upgrade::dispatchPickup(player, cobble) == 0 && cobble.count == 20, "no pickup without a Pickup upgrade");
    ctx.installUpgrade(bp, "pickup");
    ctx.check(upgrade::dispatchPickup(player, cobble) == 20, "with a Pickup upgrade the picked up items are voided");
    ctx.check(countOf(*bp, "minecraft:cobblestone") == 9, "voided items not stored");
});

ABP_REGISTER_SELFTEST("void.void_any_in_gui_next_tick", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto bp = ctx.createBackpack(Tier::Leather);
    setup(ctx, bp, "void", {"minecraft:cobblestone"}, core::FilterMode::Allow, false, true);
    auto r = bp->inventory().insertInto(3, data("minecraft:cobblestone", 12), kPlayer);
    ctx.check(r.inserted == 12, "placed in the slot first");
    co_await ctx.ticks(2);
    ctx.check(bp->inventory().at(3).empty(), "voided on the next tick");
    bp->inventory().insertInto(4, data("minecraft:dirt", 12), kPlayer);
    co_await ctx.ticks(2);
    ctx.check(countOf(*bp, "minecraft:dirt") == 12, "other items stay");
});

ABP_REGISTER_SELFTEST("void.void_overflow", [](Context& ctx) {
    auto bp = ctx.createBackpack(Tier::Leather);
    setup(ctx, bp, "advanced_void", {"minecraft:cobblestone"}, core::FilterMode::Allow, true);
    auto r = bp->inventory().insert(data("minecraft:cobblestone", 100), kPickup);
    ctx.check(r.inserted == 64 && r.voided == 36, "one slot filled, the overflow voided");
    ctx.check(slotsWith(*bp, "minecraft:cobblestone") == 1, "no second stack");
    r = bp->inventory().insert(data("minecraft:cobblestone", 10), kPickup);
    ctx.check(r.voided == 10, "full stack: everything voided");
    r = bp->inventory().insert(data("minecraft:stone", 100), kPickup);
    ctx.check(r.inserted == 100 && slotsWith(*bp, "minecraft:stone") == 2, "non-matching items fill normally");
    // A partial stack is topped up, the rest voided.
    auto bp2 = ctx.createBackpack(Tier::Leather);
    setup(ctx, bp2, "void", {"minecraft:cobblestone"}, core::FilterMode::Allow, true);
    bp2->inventory().set(5, data("minecraft:cobblestone", 30));
    r = bp2->inventory().insert(data("minecraft:cobblestone", 50), kPickup);
    ctx.check(r.inserted == 34 && r.voided == 16 && slotsWith(*bp2, "minecraft:cobblestone") == 1, "topped up to 64, 16 voided");
});

ABP_REGISTER_SELFTEST("void.void_overflow_exact_match", [](Context& ctx) {
    // Item + NBT + durability matching: new stacks may be started until a full stack exists.
    auto bp = ctx.createBackpack(Tier::Leather);
    setup(ctx, bp, "advanced_void", {"minecraft:cobblestone"}, core::FilterMode::Allow, true, false, true);
    auto r = bp->inventory().insert(data("minecraft:cobblestone", 100), kPickup);
    ctx.check(r.inserted == 100 && r.voided == 0 && slotsWith(*bp, "minecraft:cobblestone") == 2, "two stacks started");
    r = bp->inventory().insert(data("minecraft:cobblestone", 50), kPickup);
    ctx.check(r.voided == 50 && r.inserted == 0, "a full stack exists: everything voided");
});

ABP_REGISTER_SELFTEST("void.gui_overflow", [](Context& ctx) {
    auto bp = ctx.createBackpack(Tier::Leather);
    setup(ctx, bp, "void", {"minecraft:cobblestone"}, core::FilterMode::Allow, true, true);
    auto& inv = bp->inventory();
    auto  r   = inv.insertInto(0, data("minecraft:cobblestone", 60), kPlayer);
    ctx.check(r.inserted == 60, "first stack placed by hand");
    r = inv.insertInto(0, data("minecraft:cobblestone", 10), kPlayer);
    ctx.check(r.inserted == 4 && r.voided == 6, "what overflows the clicked slot is voided");
    r = inv.insertInto(1, data("minecraft:cobblestone", 10), kPlayer);
    ctx.check(r.voided == 10 && inv.at(1).empty(), "no second stack by hand either");
    // Work in GUI off: the GUI behaves normally.
    UpgradeContext uc(bp, 0);
    uc.settings()[std::string(kWorkInGuiKey)] = false;
    uc.saveSettings(true);
    r = inv.insertInto(1, data("minecraft:cobblestone", 10), kPlayer);
    ctx.check(r.inserted == 10, "work in GUI off: kept");
});

ABP_REGISTER_SELFTEST("void.void_anything_disabled", [](Context& ctx) {
    auto saved = config::get<VoidConfig>();
    config::mut<VoidConfig>().voidUpgrade.voidAnythingEnabled = false;
    auto bp = ctx.createBackpack(Tier::Leather);
    auto uc = setup(ctx, bp, "void", {"minecraft:cobblestone"});
    ctx.check(VoidSettings::read(uc).shouldVoidOverflow(), "Void Any acts as Void Overflow");
    auto r = bp->inventory().insert(data("minecraft:cobblestone", 70), kPickup);
    ctx.check(r.inserted == 64 && r.voided == 6, "overflow behaviour");
    config::mut<VoidConfig>() = saved;
});

ABP_REGISTER_SELFTEST("void.fluids_not_voided", [](Context& ctx) {
    auto bp = ctx.createBackpack(Tier::Iron);
    setup(ctx, bp, "void", {"minecraft:water_bucket"});
    ctx.installUpgrade(bp, "tank");
    auto h = api::backpack::fluidHandler(bp);
    ctx.require(h != nullptr, "fluid handler");
    int filled = h->fill(api::FluidStack{"minecraft:water", 1000}, false);
    ctx.check(filled > 0 && h->fluidInTank(0).id == "minecraft:water", "water goes into the tank");
});

ABP_REGISTER_SELFTEST("void.settings_menu", [](Context& ctx) {
    auto& mm     = ui::MenuManager::get();
    auto& player = ctx.player();
    auto  bp     = ctx.createBackpack(Tier::Leather);
    int   slot   = ctx.installUpgrade(bp, std::string(kAdvancedId));
    UpgradeContext uc(bp, slot, &player);
    auto           menu = byId(kAdvancedId)->createSettingsMenu(player, uc);
    ctx.require(menu && mm.open(player, menu), "open");
    mm.forceShown(player);
    int control = menu->size() - 9;
    (void)mm.simulate(player, {Action::Take, {Where::Menu, control + 5}, {Where::Cursor, 0}, 1});
    ctx.check(VoidSettings::read(uc).workInGui, "work in GUI toggled");
    (void)mm.simulate(player, {Action::Take, {Where::Menu, control + 6}, {Where::Cursor, 0}, 1});
    ctx.check(VoidSettings::read(uc).shouldVoidOverflow(), "Void Any -> Void Overflow");
    (void)mm.simulate(player, {Action::Take, {Where::Menu, control + 6}, {Where::Cursor, 0}, 1});
    ctx.check(!VoidSettings::read(uc).shouldVoidOverflow(), "-> Void Any");
    mm.close(player);
});

ABP_REGISTER_SELFTEST("void.native_tab", [](Context& ctx) {
    auto& p  = ctx.player();
    auto  bp = ctx.createBackpack(Tier::Leather);
    int   b  = ctx.installUpgrade(bp, "void");
    UpgradeContext uc(bp, b, &p);
    auto           t = tabData(p, uc, "void");
    ctx.check(t.value("title", "") == "Void" && t.value("tooltip", "") == "Void Settings", "tab texts");
    ctx.check(!t.value("workInGui", true) && !t.value("voidOverflow", true) && t.value("voidAnythingEnabled", false), "defaults");
    ctx.check(t["filter"].value("type", "") == "allowList" && t["filter"].value("typeState", -1) == 0, "allow list by default");
    ctx.check(tabAction(p, uc, "void", {{"op", "workInGui"}}) && VoidSettings::read(uc).workInGui, "work in GUI");
    ctx.check(tabAction(p, uc, "void", {{"op", "voidOverflow"}}) && VoidSettings::read(uc).shouldVoidOverflow(), "-> Void Overflow");
    auto saved = config::get<VoidConfig>();
    config::mut<VoidConfig>().voidUpgrade.voidAnythingEnabled = false;
    tabAction(p, uc, "void", {{"op", "voidOverflow"}}); // can't go back to Void Any
    ctx.check(VoidSettings::read(uc).shouldVoidOverflow(), "Void Any unavailable when disabled");
    ctx.check(!tabData(p, uc, "void").value("voidAnythingEnabled", true), "client told Void Any is disabled");
    config::mut<VoidConfig>() = saved;
});

} // namespace
} // namespace abp::upgrade::voiding
