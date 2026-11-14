// Void upgrade (basic + advanced), following the behaviour of VoidUpgradeWrapper:
//
// - Two modes (VoidRules.h): "Void Any" (default) and "Void Overflow"; `voidAnythingEnabled`
//   (config) set to false forces "Void Overflow".
// - Allow list by default: an empty allow list voids nothing, an empty block list everything.
// - "Work in GUI" (default off): items put in by hand are voided too ("Void Any": the slot is
//   voided on the next tick; both modes: a hand-placed second stack of a stored item, and what
//   overflows the clicked stack).
// - Items only; fluids are not voided.
// - Basic: 9 filter slots, allow/block list only. Advanced: 16 slots, item/mod/tag match,
//   durability and NBT.

#include "upgrade/void/VoidUpgrade.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"

#include "game/Backpack.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "ui/menu/BackpackMenu.h"
#include "ui/menu/FilterMenu.h"
#include "net/NativeScreens.h"
#include "upgrade/UpgradeManager.h"
#include "upgrade/filter/FilterCommon.h"

ABP_REGISTER_CONFIG("void", abp::upgrade::voiding::VoidConfig);

namespace abp::upgrade::voiding {

// ---- settings -----------------------------------------------------------------------------------

VoidTierConfig tierConfig(bool advanced) {
    auto const& cfg = config::get<VoidConfig>();
    auto        c   = advanced ? cfg.advancedVoidUpgrade : cfg.voidUpgrade;
    c.filterSlots   = std::clamp(c.filterSlots, 1, 20);
    c.slotsInRow    = std::clamp(c.slotsInRow, 1, 6);
    return c;
}

VoidSettings VoidSettings::read(UpgradeContext const& ctx) {
    VoidSettings s;
    auto const&  j      = ctx.settings();
    auto*        type   = upgradeFor(ctx.state().item);
    s.advanced          = type && type->info().id == kAdvancedId;
    auto tc             = tierConfig(s.advanced);
    s.voidAnythingEnabled = tc.voidAnythingEnabled;
    s.filter              = readFilter(j, s.advanced);
    if (j.is_object()) {
        if (auto it = j.find(kVoidOverflowKey); it != j.end() && it->is_boolean()) {
            s.storedOverflow = it->get<bool>();
        } else if (auto vt = j.find("voidType"); vt != j.end() && vt->is_string()) {
            // Settings saved by earlier builds with a mode name: every mode except "always" voided
            // overflow only.
            s.storedOverflow = vt->get<std::string>() != "always";
        } else if (auto old = j.find("overflow"); old != j.end() && old->is_boolean()) {
            s.storedOverflow = old->get<bool>(); // phase-1 flag
        }
        if (auto it = j.find(kWorkInGuiKey); it != j.end() && it->is_boolean()) s.workInGui = it->get<bool>();
    }
    return s;
}

core::Filter readFilter(nlohmann::json const& settings, bool advanced) {
    int  slots  = tierConfig(advanced).filterSlots;
    auto stored = settings.is_object() ? settings.value(std::string(kFilterKey), nlohmann::json()) : nlohmann::json();
    auto f      = core::Filter::fromJson(stored, slots, core::FilterDefaults{.allowList = true});
    if (stored.is_object() && !stored.empty() && !stored.contains("v") && f.mode == core::FilterMode::Deny && f.isEmpty()) {
        f.mode = core::FilterMode::Allow; // phase 1: an empty list never voided anything
    }
    filtering::normalize(f, advanced ? UpgradeTier::Advanced : UpgradeTier::Basic, slots);
    return f;
}

filtering::FilterSpec filterSpec(bool advanced) {
    auto tc = tierConfig(advanced);
    return filtering::FilterSpec{tc.filterSlots, tc.slotsInRow, advanced, filtering::TypeButton::AllowList};
}

bool VoidSettings::matches(core::ItemStackData const& item) const {
    return !item.empty() && filter.passes(item, &filtering::matcher());
}

VoidRules VoidSettings::rules() const {
    VoidRules r;
    r.overflowOnly    = shouldVoidOverflow();
    r.workInGui       = workInGui;
    r.matchBy         = filter.matchBy;
    r.matchDurability = filter.matchDurability;
    r.matchNbt        = filter.matchNbt;
    r.matches         = [s = *this](core::ItemStackData const& item) { return s.matches(item); };
    return r;
}

namespace {

/// True while `ctx` still points at a Void upgrade (a settings menu can outlive it).
bool alive(UpgradeContext const& ctx) {
    auto const& slots = ctx.backpack().data().upgrades;
    if (ctx.slot() < 0 || ctx.slot() >= static_cast<int>(slots.size())) return false;
    auto* type = upgradeFor(slots[static_cast<std::size_t>(ctx.slot())].item);
    return type && (type->info().id == kBasicId || type->info().id == kAdvancedId);
}

void writeSetting(UpgradeContext const& ctx, std::string_view key, nlohmann::json value) {
    if (!alive(ctx)) return;
    auto& j = ctx.settings();
    if (!j.is_object()) j = nlohmann::json::object();
    j[std::string(key)] = std::move(value);
    ctx.saveSettings(true);
}

/// Toggles the mode (`setShouldVoidOverflow(!shouldVoidOverflow())`): switching back to
/// "Void Any" is ignored while the config disables it.
void toggleOverflow(UpgradeContext const& ctx) {
    auto s    = VoidSettings::read(ctx);
    bool next = !s.shouldVoidOverflow();
    if (!next && !s.voidAnythingEnabled) return;
    if (!alive(ctx)) return;
    auto& j = ctx.settings();
    if (!j.is_object()) j = nlohmann::json::object();
    j.erase("voidType");
    j.erase("overflow");
    j[std::string(kVoidOverflowKey)] = next;
    ctx.saveSettings(true);
}

// ---- "work in GUI" with "Void Any": void the slot on the next tick (onSlotChange + tick) --

struct GuiVoider : std::enable_shared_from_this<GuiVoider> {
    std::weak_ptr<game::Backpack> backpack;
    VoidSettings                  settings;
    std::set<int>                 slots;
    bool                          scheduled = false;

    void onSlotChange(core::BackpackInventory const& inv, int slot);
    void drain();
};

std::vector<std::weak_ptr<GuiVoider>> gScheduled;

void GuiVoider::onSlotChange(core::BackpackInventory const& inv, int slot) {
    if (!settings.workInGui || settings.shouldVoidOverflow()) return;
    if (slot < 0 || slot >= inv.size() || !settings.matches(inv.at(slot))) return;
    slots.insert(slot);
    if (!scheduled) {
        scheduled = true;
        gScheduled.push_back(weak_from_this());
    }
}

void GuiVoider::drain() {
    scheduled = false;
    auto bp   = backpack.lock();
    if (!bp || slots.empty()) return;
    auto& inv = bp->inventory();
    for (int slot : slots) {
        // Only what still matches (the slot may have changed since it was queued).
        if (slot < inv.size() && settings.matches(inv.at(slot))) {
            (void)inv.extract(slot, inv.at(slot).count, core::AccessContext{core::Source::Upgrade});
        }
    }
    slots.clear();
}

class VoidScheduler {
public:
    void install() {
        mTick = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>([](auto&) {
            if (gScheduled.empty()) return;
            auto list = std::move(gScheduled);
            gScheduled.clear();
            for (auto const& w : list) {
                if (auto v = w.lock()) v->drain();
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

std::pair<std::string, std::vector<std::string>> modeText(Player& player, bool overflow, bool anythingEnabled) {
    if (!overflow) return {trFor(player, "abp.void.button.void_any", "Void Any"), {}};
    std::vector<std::string> lore{
        "§7" + trFor(player, "abp.void.button.void_overflow.detail", "Lets one slot fill up with the item,\nthen voids any extra that arrives")
    };
    if (!anythingEnabled) {
        lore.push_back("§c" + trFor(player, "abp.void.button.void_anything_disabled", "Voiding everything is turned off on this server"));
    }
    return {trFor(player, "abp.void.button.void_overflow", "Void Overflow"), lore};
}

class VoidMenu : public ui::FilterMenu {
public:
    VoidMenu(std::string title, UpgradeContext ctx, Options options)
    : FilterMenu(
          std::move(title),
          [ctx] { return alive(ctx) ? VoidSettings::read(ctx).filter : core::Filter(9, core::FilterDefaults{.allowList = true}); },
          [ctx](core::Filter const& f) { writeSetting(ctx, kFilterKey, f.toJson()); },
          std::move(options)
      ),
      mCtx(std::move(ctx)) {}

    void build(Player& player) override {
        FilterMenu::build(player);
        if (!alive(mCtx)) return;
        int  control = size() - 9;
        auto s       = VoidSettings::read(mCtx);
        setSlot(
            control + 5,
            std::make_shared<ui::ButtonSlot>(
                ui::icon(
                    "minecraft:crafting_table",
                    s.workInGui ? trFor(player, "abp.void.button.works_in_gui", "Works in GUI as well")
                                : trFor(player, "abp.void.button.only_automatic", "Only works with other upgrades/automation")
                ),
                [this](ui::ClickContext const&) {
                    writeSetting(mCtx, kWorkInGuiKey, !VoidSettings::read(mCtx).workInGui);
                    invalidate();
                }
            )
        );
        auto text = modeText(player, s.shouldVoidOverflow(), s.voidAnythingEnabled);
        setSlot(
            control + 6,
            std::make_shared<ui::ButtonSlot>(ui::icon("minecraft:lava_bucket", text.first, text.second), [this](ui::ClickContext const&) {
                toggleOverflow(mCtx);
                invalidate();
            })
        );
    }

private:
    UpgradeContext mCtx;
};

// ---- upgrades ---------------------------------------------------------------------------------

class VoidUpgradeBase : public IUpgrade {
public:
    void attach(UpgradeContext const& ctx, core::BackpackInventory& inventory) override {
        auto s = VoidSettings::read(ctx);
        inventory.addInsertFilter(ctx.hookOwner(), kInsertPriority, [r = s.rules()](core::InsertQuery const& q) { return verdict(r, q); });
        auto gui      = std::make_shared<GuiVoider>();
        gui->backpack = ctx.backpackPtr();
        gui->settings = s;
        inventory.addInsertListener(ctx.hookOwner(), [gui](int slot, core::AccessContext const& ac) {
            if (ac.source != core::Source::Player) return;
            if (auto bp = gui->backpack.lock()) gui->onSlotChange(bp->inventory(), slot);
        });
        inventory.addExtractListener(ctx.hookOwner(), [gui](int slot, core::ItemStackData const&, core::AccessContext const& ac) {
            if (ac.source != core::Source::Player) return;
            if (auto bp = gui->backpack.lock()) gui->onSlotChange(bp->inventory(), slot);
        });
    }

    std::shared_ptr<ui::Menu> createSettingsMenu(Player& player, UpgradeContext const& ctx) override {
        bool                    advanced = info().id == kAdvancedId;
        ui::FilterMenu::Options options;
        options.allowMatchBy    = advanced;
        options.allowNbt        = advanced;
        options.allowDurability = advanced;
        options.onBack          = [bp = ctx.backpackPtr()](Player& p) {
            ui::BackpackMenu::openBackpack(p, bp, {}, ui::BackpackMenu::View::Upgrades);
        };
        options.slots = tierConfig(advanced).filterSlots;
        return std::make_shared<VoidMenu>(displayName(*this, &player), ctx.withPlayer(nullptr), std::move(options));
    }

    [[nodiscard]] std::vector<std::string> describe(Player& player, UpgradeContext const& ctx) const override {
        auto s = VoidSettings::read(ctx);
        return {"§7" + modeText(player, s.shouldVoidOverflow(), true).first};
    }
};

// ---- native tab (client: client/ui/tabs/void) --------------------------------------------------

ABP_REGISTER_TAB_DATA(net::TabDataProvider{
    "void",
    [](Player& player, UpgradeContext const& ctx) {
        auto s = VoidSettings::read(ctx);
        return nlohmann::json{
            {"title",               s.advanced ? trFor(player, "abp.void.tab.advanced", "Adv. Void") : trFor(player, "abp.void.tab", "Void")},
            {"tooltip",             s.advanced ? trFor(player, "abp.void.tab.advanced.tooltip", "Advanced Void Settings")
                                               : trFor(player, "abp.void.tab.tooltip", "Void Settings")},
            {"workInGui",           s.workInGui},
            {"voidOverflow",        s.shouldVoidOverflow()},
            {"voidAnythingEnabled", s.voidAnythingEnabled},
            {"filter",              filtering::tabJson(s.filter, filterSpec(s.advanced))},
        };
    },
});

ABP_REGISTER_TAB_ACTION(net::TabActionHandler{
    "void",
    [](Player& player, UpgradeContext const& ctx, nlohmann::json const& data) {
        if (!data.is_object() || !alive(ctx)) return false;
        auto s = VoidSettings::read(ctx);
        if (data.value("filter", std::string{}) == "filter") {
            auto spec = filterSpec(s.advanced);
            if (filtering::applyAction(s.filter, spec, data, filtering::cursorOf(player))) {
                filtering::normalize(s.filter, s.advanced ? UpgradeTier::Advanced : UpgradeTier::Basic, spec.slots);
                writeSetting(ctx, kFilterKey, s.filter.toJson());
            }
            return true;
        }
        auto op = data.value("op", std::string{});
        if (op == "workInGui") {
            writeSetting(ctx, kWorkInGuiKey, !s.workInGui);
            return true;
        }
        if (op == "voidOverflow") {
            toggleOverflow(ctx);
            return true;
        }
        return false;
    },
});

class VoidUpgrade final : public VoidUpgradeBase {
public:
    [[nodiscard]] UpgradeInfo const& info() const override {
        static UpgradeInfo const i{
            .id             = std::string(kBasicId),
            .itemId         = "alleviatedbackpacks:void_upgrade",
            .nameKey        = "abp.void.name",
            .nameFallback   = "Void Upgrade",
            .tier           = UpgradeTier::Basic,
            .group          = "void",
            .maxPerBackpack = kUnlimitedPerBackpack, // VoidUpgradeItem: no conflicts / limit
            .hasSettings    = true,
        };
        return i;
    }
};

class AdvancedVoidUpgrade final : public VoidUpgradeBase {
public:
    [[nodiscard]] UpgradeInfo const& info() const override {
        static UpgradeInfo const i{
            .id             = std::string(kAdvancedId),
            .itemId         = "alleviatedbackpacks:advanced_void_upgrade",
            .nameKey        = "abp.void.advanced_name",
            .nameFallback   = "Advanced Void Upgrade",
            .tier           = UpgradeTier::Advanced,
            .group          = "void",
            .maxPerBackpack = kUnlimitedPerBackpack,
            .hasSettings    = true,
        };
        return i;
    }
};

} // namespace
} // namespace abp::upgrade::voiding

ABP_REGISTER_UPGRADE(abp::upgrade::voiding::VoidUpgrade);
ABP_REGISTER_UPGRADE(abp::upgrade::voiding::AdvancedVoidUpgrade);
ABP_REGISTER_MODULE(abp::upgrade::voiding::VoidScheduler);
