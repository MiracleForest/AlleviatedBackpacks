// Compacting upgrade (basic: 2x2 recipes; advanced: 3x3 then 2x2), following the behaviour of
// CompactingUpgradeWrapper:
//
// - Items inserted by anything but the GUI (pickup, magnet, automation, other upgrades) are
//   compacted right after the insert, using the game's own crafting recipes for a full n x n grid
//   of the item; inserting the result compacts it in turn (nuggets -> ingots -> blocks).
// - "Compact Only Uncraftable" (default): only recipes whose result crafts back into the items;
//   "Compact Anything" uses any recipe.
// - "Works in GUI as well" (default off): slots changed in the GUI are compacted on the next tick;
//   otherwise GUI changes are ignored.
// - A craft only happens when its result (and any leftovers) fit, checked before taking the inputs.

#include "upgrade/compacting/Compacting.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"

#include "game/Backpack.h"
#include "game/ItemConv.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "ui/menu/BackpackMenu.h"
#include "ui/menu/FilterMenu.h"
#include "net/NativeScreens.h"
#include "upgrade/UpgradeManager.h"
#include "upgrade/filter/FilterCommon.h"

ABP_REGISTER_CONFIG("compacting", abp::upgrade::compacting::CompactingConfig);

namespace abp::upgrade::compacting {

int filterSlots(bool advanced) {
    auto const& c = config::get<CompactingConfig>();
    return std::clamp(advanced ? c.advancedCompactingUpgrade.filterSlots : c.compactingUpgrade.filterSlots, 1, 20);
}

int slotsInRow(bool advanced) {
    auto const& c = config::get<CompactingConfig>();
    return std::clamp(advanced ? c.advancedCompactingUpgrade.slotsInRow : c.compactingUpgrade.slotsInRow, 1, 6);
}

CompactingSettings CompactingSettings::read(nlohmann::json const& settings, bool advanced) {
    CompactingSettings s;
    s.advanced    = advanced;
    s.filter      = filtering::read(settings, filterSlots(advanced), {}, "compacting"); // block list by default
    filtering::normalize(s.filter, advanced ? UpgradeTier::Advanced : UpgradeTier::Basic, filterSlots(advanced));
    if (settings.is_object()) {
        if (auto it = settings.find(kCompactAnythingKey); it != settings.end() && it->is_boolean()) {
            s.compactAnything = it->get<bool>();
        } else if (auto old = settings.find("onlyUncraftable"); old != settings.end() && old->is_boolean()) {
            s.compactAnything = !old->get<bool>(); // phase-1 key
        }
        if (auto it = settings.find(kWorkInGuiKey); it != settings.end() && it->is_boolean()) s.workInGui = it->get<bool>();
    }
    return s;
}

filtering::FilterSpec filterSpec(bool advanced) {
    return filtering::FilterSpec{filterSlots(advanced), slotsInRow(advanced), advanced, filtering::TypeButton::AllowList};
}

namespace {

/// One craft of a full `grid` x `grid` square of `item` (getCompactingResult).
std::optional<CompactingCraft> craftFor(core::ItemStackData const& item, int grid) {
    auto c = findCompaction(item, grid);
    if (!c || c->result.empty()) return std::nullopt;
    return CompactingCraft{c->result, grid * grid, c->remaining};
}

} // namespace

std::optional<CompactingCraft> compactingCraft(core::ItemStackData const& item, bool advanced, bool compactAnything) {
    if (item.empty()) return std::nullopt;
    auto shapes = compactingShapes(item);
    if (advanced && (shapes.threeByThreeUncraftable || (compactAnything && shapes.threeByThree))) return craftFor(item, 3);
    if (shapes.twoByTwoUncraftable || (compactAnything && shapes.twoByTwo)) return craftFor(item, 2);
    return std::nullopt;
}

bool canCompact(core::ItemStackData const& item) { return !item.empty() && compactingShapes(item).any(); }

namespace {

core::AccessContext const kUpgradeAccess{core::Source::Upgrade};

/// Runtime state of one installed compacting upgrade (the wrapper fields). Owned by its
/// inventory listeners, so it dies with the hooks (rebuild, disable, removal); the scheduler only
/// keeps weak references.
struct Engine : std::enable_shared_from_this<Engine> {
    std::weak_ptr<game::Backpack> backpack;
    CompactingSettings            settings;
    std::set<int>                 slotsToCompact;
    bool                          scheduled = false;

    [[nodiscard]] bool matches(core::ItemStackData const& item) const {
        return !item.empty() && settings.filter.passes(item, &filtering::matcher());
    }

    /// `onAfterInsert`. Inserting a result fires this again for the result's slot
    /// (nested insert listeners), which is how results compact further.
    void onAfterInsert(core::BackpackInventory& inv, int slot) {
        try {
            compactSlot(inv, slot);
        } catch (std::exception const& e) {
            log().error("compacting failed: {}", e.what());
        }
    }

    /// `onSlotChange` (a slot changed through the GUI): only with "Works in GUI as well".
    void onSlotChange(int slot) {
        if (!settings.workInGui) return;
        slotsToCompact.insert(slot);
        schedule();
    }

    void schedule();

    /// `tick`: the slots queued by the GUI.
    void drain() {
        scheduled = false;
        auto bp   = backpack.lock();
        if (!bp || slotsToCompact.empty()) return;
        auto slots = std::move(slotsToCompact);
        slotsToCompact.clear();
        for (int slot : slots) onAfterInsert(bp->inventory(), slot);
    }

    void compactSlot(core::BackpackInventory& inv, int slot) {
        if (slot < 0 || slot >= inv.size()) return;
        auto stack = inv.at(slot);
        if (stack.empty() || !matches(stack)) return;
        if (auto craft = compactingCraft(stack, settings.advanced, settings.compactAnything)) tryCompacting(inv, stack, *craft);
    }

    /// `fitsResultAndRemainingItems`: without leftovers a simulated insert of the result;
    /// with them the result and then the leftovers go into a copy of the slots, slot by slot (each
    /// slot's real limit and acceptance).
    [[nodiscard]] static bool
    fits(core::BackpackInventory& inv, core::ItemStackData const& result, std::vector<core::ItemStackData> const& remaining = {}) {
        if (remaining.empty()) return inv.insert(result, kUpgradeAccess, true).remainder.empty();
        std::vector<core::ItemStackData> slots;
        slots.reserve(static_cast<std::size_t>(inv.size()));
        for (int i = 0; i < inv.size(); ++i) slots.push_back(inv.at(i));
        auto place = [&](core::ItemStackData item) {
            for (int i = 0; i < inv.size() && item.count > 0; ++i) {
                auto& s = slots[static_cast<std::size_t>(i)];
                if (s.empty()) {
                    if (!inv.at(i).empty()) continue;
                    auto r = inv.insertInto(i, item, kUpgradeAccess, true);
                    int  n = r.inserted + r.voided;
                    if (n <= 0) continue;
                    if (r.inserted > 0) s = item.withCount(r.inserted);
                    item.count -= n;
                } else if (s.sameItem(item)) {
                    int room = inv.slotLimit(i, item) - s.count;
                    if (room <= 0) continue;
                    int n       = std::min(room, item.count);
                    s.count    += n;
                    item.count -= n;
                }
            }
            return item.count <= 0;
        };
        if (!place(result)) return false;
        for (auto const& r : remaining) {
            if (!place(r)) return false;
        }
        return true;
    }

    /// `InventoryHelper.extractFromInventory`: up to `amount` of `item` from the slots in
    /// order. Returns how many were (or would be) taken; `taken` records them for a roll-back.
    static int extract(
        core::BackpackInventory&                           inv,
        core::ItemStackData const&                         item,
        int                                                amount,
        bool                                               simulate,
        std::vector<std::pair<int, core::ItemStackData>>* taken = nullptr
    ) {
        int got = 0;
        for (int i = 0; i < inv.size() && got < amount; ++i) {
            if (!inv.at(i).sameItem(item)) continue;
            auto e = inv.extract(i, amount - got, kUpgradeAccess, simulate);
            if (e.empty()) continue;
            got += e.count;
            if (taken) taken->emplace_back(i, std::move(e));
        }
        return got;
    }

    /// `tryCompacting`: crafts while a simulated extraction yields a full set and the
    /// result fits; stops at the first craft that would not fit.
    void tryCompacting(core::BackpackInventory& inv, core::ItemStackData const& stack, CompactingCraft const& craft) {
        int total = craft.count;
        if (craft.result.empty() || total <= 0) return;
        for (int guard = 0; extract(inv, stack, total, true) == total && guard < 100000; ++guard) {
            if (!fits(inv, craft.result, craft.remaining)) return;
            std::vector<std::pair<int, core::ItemStackData>> taken;
            if (extract(inv, stack, total, false, &taken) != total) {
                // Should not happen after the simulation; never create a result from a partial set.
                for (auto const& [s, e] : taken) {
                    auto rest = inv.insertInto(s, e, core::AccessContext{core::Source::Internal}).remainder;
                    if (!rest.empty()) (void)inv.insert(rest, core::AccessContext{core::Source::Internal});
                }
                return;
            }
            (void)inv.insert(craft.result, kUpgradeAccess);
            // InventoryHelper.insertIntoInventory(remainingItems) after the result.
            for (auto const& left : craft.remaining) (void)inv.insert(left, kUpgradeAccess);
        }
    }
};

/// Engines with queued slots, drained on the next server tick.
std::vector<std::weak_ptr<Engine>> gScheduled;

void Engine::schedule() {
    if (scheduled || slotsToCompact.empty()) return;
    scheduled = true;
    gScheduled.push_back(weak_from_this());
}

class CompactingScheduler {
public:
    void install() {
        mTick = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>([](auto&) {
            if (gScheduled.empty()) return;
            auto list = std::move(gScheduled);
            gScheduled.clear();
            for (auto const& weak : list) {
                if (auto engine = weak.lock()) engine->drain();
            }
        });
    }
    void uninstall() {
        ll::event::EventBus::getInstance().removeListener(mTick);
        gScheduled.clear();
    }

private:
    ll::event::ListenerPtr mTick;
};

// ---- settings menu (chest fallback) ---------------------------------------------------------------

bool alive(UpgradeContext const& ctx) {
    auto const& slots = ctx.backpack().data().upgrades;
    if (ctx.slot() < 0 || ctx.slot() >= static_cast<int>(slots.size())) return false;
    auto* type = upgradeFor(slots[static_cast<std::size_t>(ctx.slot())].item);
    return type && type->info().group == "compacting";
}

void writeSetting(UpgradeContext const& ctx, std::string_view key, nlohmann::json value) {
    if (!alive(ctx)) return;
    auto& j = ctx.settings();
    if (!j.is_object()) j = nlohmann::json::object();
    j[std::string(key)] = std::move(value);
    ctx.saveSettings(true);
}

class CompactingMenu : public ui::FilterMenu {
public:
    static constexpr int kUncraftableOffset = 5;
    static constexpr int kWorkInGuiOffset   = 6;

    CompactingMenu(std::string title, UpgradeContext ctx, bool advanced, Options options)
    : FilterMenu(
          std::move(title),
          [ctx, advanced] { return CompactingSettings::read(ctx.settings(), advanced).filter; },
          [ctx](core::Filter const& f) { writeSetting(ctx, kFilterKey, f.toJson()); },
          std::move(options)
      ),
      mCtx(std::move(ctx)),
      mAdvanced(advanced) {}

    void build(Player& player) override {
        FilterMenu::build(player);
        if (!alive(mCtx)) return;
        int  control = size() - 9;
        auto s       = CompactingSettings::read(mCtx.settings(), mAdvanced);
        setSlot(
            control + kUncraftableOffset,
            std::make_shared<ui::ButtonSlot>(
                ui::icon(
                    s.compactAnything ? "minecraft:piston" : "minecraft:crafting_table",
                    s.compactAnything ? trFor(player, "abp.compacting.button.compact_anything", "Compact Anything")
                                      : trFor(player, "abp.compacting.button.compact_only_uncraftable", "Compact Only Uncraftable")
                ),
                [this](ui::ClickContext const&) {
                    writeSetting(mCtx, kCompactAnythingKey, !CompactingSettings::read(mCtx.settings(), mAdvanced).compactAnything);
                    invalidate();
                }
            )
        );
        setSlot(
            control + kWorkInGuiOffset,
            std::make_shared<ui::ButtonSlot>(
                ui::icon(
                    "minecraft:chest",
                    s.workInGui ? trFor(player, "abp.compacting.button.works_in_gui", "Works in GUI as well")
                                : trFor(player, "abp.compacting.button.only_automatic", "Only works with other upgrades/automation")
                ),
                [this](ui::ClickContext const&) {
                    writeSetting(mCtx, kWorkInGuiKey, !CompactingSettings::read(mCtx.settings(), mAdvanced).workInGui);
                    invalidate();
                }
            )
        );
    }

private:
    UpgradeContext mCtx;
    bool           mAdvanced;
};

// ---- upgrades ------------------------------------------------------------------------------------

class CompactingUpgradeBase : public IUpgrade {
public:
    [[nodiscard]] virtual bool advanced() const = 0;

    void attach(UpgradeContext const& ctx, core::BackpackInventory& inventory) override {
        auto engine      = std::make_shared<Engine>();
        engine->backpack = ctx.backpackPtr();
        engine->settings = CompactingSettings::read(ctx.settings(), advanced());
        inventory.addInsertListener(ctx.hookOwner(), [engine](int slot, core::AccessContext const& ac) {
            auto bp = engine->backpack.lock();
            if (!bp) return;
            if (ac.source == core::Source::Player) engine->onSlotChange(slot);
            else engine->onAfterInsert(bp->inventory(), slot);
        });
        inventory.addExtractListener(ctx.hookOwner(), [engine](int slot, core::ItemStackData const&, core::AccessContext const& ac) {
            if (ac.source == core::Source::Player) engine->onSlotChange(slot);
        });
    }

    std::shared_ptr<ui::Menu> createSettingsMenu(Player& player, UpgradeContext const& ctx) override {
        ui::FilterMenu::Options options;
        options.slots           = filterSlots(advanced());
        options.allowMatchBy    = advanced();
        options.allowNbt        = advanced();
        options.allowDurability = advanced();
        options.onBack          = [bp = ctx.backpackPtr()](Player& p) {
            ui::BackpackMenu::openBackpack(p, bp, {}, ui::BackpackMenu::View::Upgrades);
        };
        return std::make_shared<CompactingMenu>(displayName(*this, &player), ctx, advanced(), std::move(options));
    }

    [[nodiscard]] std::vector<std::string> describe(Player& player, UpgradeContext const& ctx) const override {
        auto s = CompactingSettings::read(ctx.settings(), advanced());
        return {
            "§7"
            + (s.compactAnything ? trFor(player, "abp.compacting.button.compact_anything", "Compact Anything")
                                 : trFor(player, "abp.compacting.button.compact_only_uncraftable", "Compact Only Uncraftable"))
        };
    }
};

// ---- native tab (client: client/ui/tabs/compacting) --------------------------------------------

bool isAdvancedCompacting(UpgradeContext const& ctx) {
    auto* type = upgradeFor(ctx.state().item);
    return type && type->info().id == kAdvancedId;
}

ABP_REGISTER_TAB_DATA(net::TabDataProvider{
    "compacting",
    [](Player& player, UpgradeContext const& ctx) {
        bool adv = isAdvancedCompacting(ctx);
        auto s   = CompactingSettings::read(ctx.settings(), adv);
        return nlohmann::json{
            {"title",           adv ? trFor(player, "abp.compacting.tab.advanced", "Adv. Compacting") : trFor(player, "abp.compacting.tab", "Compa...")},
            {"tooltip",         adv ? trFor(player, "abp.compacting.tab.advanced.tooltip", "Advanced Compacting Settings")
                                    : trFor(player, "abp.compacting.tab.tooltip", "Compacting Settings")},
            {"compactAnything", s.compactAnything},
            {"workInGui",       s.workInGui},
            {"filter",          filtering::tabJson(s.filter, filterSpec(adv))},
        };
    },
});

ABP_REGISTER_TAB_ACTION(net::TabActionHandler{
    "compacting",
    [](Player& player, UpgradeContext const& ctx, nlohmann::json const& data) {
        if (!data.is_object() || !alive(ctx)) return false;
        bool adv = isAdvancedCompacting(ctx);
        auto s   = CompactingSettings::read(ctx.settings(), adv);
        if (data.value("filter", std::string{}) == "filter") {
            auto cursor = filtering::cursorOf(player);
            // FilterLogic(isItemValid): only items with a compacting shape go into the filter.
            if (data.value("op", std::string{}) == "slot" && !cursor.empty() && !canCompact(cursor)) return true;
            auto spec = filterSpec(adv);
            if (filtering::applyAction(s.filter, spec, data, cursor)) {
                filtering::normalize(s.filter, adv ? UpgradeTier::Advanced : UpgradeTier::Basic, spec.slots);
                writeSetting(ctx, kFilterKey, s.filter.toJson());
            }
            return true;
        }
        auto op = data.value("op", std::string{});
        if (op == "compactAnything") {
            writeSetting(ctx, kCompactAnythingKey, !s.compactAnything);
            return true;
        }
        if (op == "workInGui") {
            writeSetting(ctx, kWorkInGuiKey, !s.workInGui);
            return true;
        }
        return false;
    },
});

class CompactingUpgrade final : public CompactingUpgradeBase {
public:
    [[nodiscard]] UpgradeInfo const& info() const override {
        static UpgradeInfo const i{
            .id             = std::string(kBasicId),
            .itemId         = "alleviatedbackpacks:compacting_upgrade",
            .nameKey        = "abp.compacting.name",
            .nameFallback   = "Compacting Upgrade",
            .tier           = UpgradeTier::Basic,
            .group          = "compacting",
            .maxPerBackpack = kUnlimitedPerBackpack, // no per-type limit
            .hasSettings    = true,
        };
        return i;
    }
    [[nodiscard]] bool advanced() const override { return false; }
};

class AdvancedCompactingUpgrade final : public CompactingUpgradeBase {
public:
    [[nodiscard]] UpgradeInfo const& info() const override {
        static UpgradeInfo const i{
            .id             = std::string(kAdvancedId),
            .itemId         = "alleviatedbackpacks:advanced_compacting_upgrade",
            .nameKey        = "abp.compacting.advanced_name",
            .nameFallback   = "Advanced Compacting Upgrade",
            .tier           = UpgradeTier::Advanced,
            .group          = "compacting",
            .maxPerBackpack = kUnlimitedPerBackpack,
            .hasSettings    = true,
        };
        return i;
    }
    [[nodiscard]] bool advanced() const override { return true; }
};

} // namespace
} // namespace abp::upgrade::compacting

ABP_REGISTER_UPGRADE(abp::upgrade::compacting::CompactingUpgrade);
ABP_REGISTER_UPGRADE(abp::upgrade::compacting::AdvancedCompactingUpgrade);
ABP_REGISTER_MODULE(abp::upgrade::compacting::CompactingScheduler);
