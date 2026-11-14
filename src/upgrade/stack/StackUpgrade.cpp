// Stack upgrades (Starter, Tier 1-4, Omega) and Stack downgrades (Tier 1-3), after
// StackUpgradeItem / ModItems: per-slot limit multipliers. Several stack upgrades in
// one backpack multiply each other. Removing, swapping or installing one is refused when a slot
// (or a tank / battery) would hold more than the new limit allows.

#include "upgrade/stack/StackUpgrade.h"

#include <algorithm>
#include <climits>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>
#include <unordered_map>

#include "mc/world/actor/player/Player.h"

#include "api/BackpackCapabilities.h"
#include "core/Capacity.h"

#include "core/Codec.h"
#include "core/StackMultiplier.h"
#include "game/BackpackItem.h"
#include "game/ItemConv.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "upgrade/UpgradeManager.h"

ABP_REGISTER_CONFIG("stack", abp::upgrade::stack::StackConfig);

namespace abp::upgrade::stack {

namespace {

constexpr TierDef kTiers[] = {
    {"stack_starter",          "Stack Upgrade Starter Tier", &StackConfig::starterTier   },
    {"stack_tier_1",           "Stack Upgrade Tier 1",       &StackConfig::tier1         },
    {"stack_tier_2",           "Stack Upgrade Tier 2",       &StackConfig::tier2         },
    {"stack_tier_3",           "Stack Upgrade Tier 3",       &StackConfig::tier3         },
    {"stack_tier_4",           "Stack Upgrade Tier 4",       &StackConfig::tier4         },
    {"stack_omega",            "Stack Upgrade Omega Tier",   &StackConfig::omegaTier     },
    {"stack_downgrade_tier_1", "Stack Downgrade Tier 1",     &StackConfig::downgradeTier1},
    {"stack_downgrade_tier_2", "Stack Downgrade Tier 2",     &StackConfig::downgradeTier2},
    {"stack_downgrade_tier_3", "Stack Downgrade Tier 3",     &StackConfig::downgradeTier3},
};

constexpr std::string_view kGroup = "stack";

std::string msg(Player* player, std::string_view key, std::string_view fallback, std::string const& arg) {
    return player ? trFor(*player, key, fallback, arg) : tr(key, fallback, arg);
}

/// A double as Java's `String.valueOf(double)` prints it (the mod passes the multiplier to the
/// message as a double): "2.0", "1.5", "0.125", "2.147483647E9".
std::string javaDouble(double v) {
    if (!std::isfinite(v)) return std::isnan(v) ? "NaN" : (v > 0 ? "Infinity" : "-Infinity");
    double a = std::fabs(v);
    char   buf[64];
    if (a == 0.0 || (a >= 1e-3 && a < 1e7)) {
        auto r = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::fixed);
        std::string s(buf, r.ptr);
        if (s.find('.') == std::string::npos) s += ".0";
        return s;
    }
    auto        r = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::scientific);
    std::string s(buf, r.ptr);
    auto        e        = s.find('e');
    std::string mantissa = s.substr(0, e);
    if (mantissa.find('.') == std::string::npos) mantissa += ".0";
    int exponent = std::stoi(s.substr(e + 1));
    return mantissa + "E" + std::to_string(exponent);
}

/// Bumped by `/backpack reload`: invalidates the caches below and the multipliers captured by
/// limit modifiers. Server thread only.
std::uint64_t gGeneration = 1;

struct AffectsCache {
    std::uint64_t                         generation = 0;
    std::unordered_map<std::string, bool> byId;
};

std::array<std::string, std::size(kTiers)> const& tierItemIds() {
    static auto const ids = [] {
        std::array<std::string, std::size(kTiers)> out;
        for (std::size_t i = 0; i < out.size(); ++i) out[i] = itemIdOf(kTiers[i].id);
        return out;
    }();
    return ids;
}

/// Applies config changes after `/backpack reload`.
class StackModule {
public:
    void install() { config::onReload([] { ++gGeneration; }); }
    void uninstall() {}
};

} // namespace

std::span<TierDef const>       tiers() { return kTiers; }

std::string itemIdOf(std::string_view upgradeId) {
    return std::string(game::kNamespace) + ":" + std::string(upgradeId) + "_upgrade";
}

TierDef const* tierById(std::string_view upgradeId) {
    for (auto const& t : kTiers)
        if (t.id == upgradeId) return &t;
    return nullptr;
}

TierDef const* tierByItem(std::string_view itemId) {
    auto const& ids = tierItemIds();
    for (std::size_t i = 0; i < ids.size(); ++i)
        if (ids[i] == itemId) return &kTiers[i];
    return nullptr;
}

double multiplierOf(TierDef const& tier) {
    double m = config::get<StackConfig>().*tier.multiplier;
    return (m > 0.0 && std::isfinite(m)) ? m : 1.0;
}

bool affects(core::ItemStackData const& item) {
    // Called for every slot limit lookup: memoize per item id (cleared on config reload).
    static AffectsCache cache;
    if (cache.generation != gGeneration) {
        cache.byId.clear();
        cache.generation = gGeneration;
    }
    if (auto it = cache.byId.find(item.id); it != cache.byId.end()) return it->second;
    bool result = true;
    for (auto const& pattern : config::get<StackConfig>().nonStackableItems) {
        if (core::globMatch(pattern, item.id)) {
            result = false;
            break;
        }
    }
    cache.byId.emplace(item.id, result);
    return result;
}

namespace {
std::vector<double> multipliersOf(game::Backpack const& backpack, int skipSlot, double extra) {
    std::vector<double> ms;
    auto const&         slots = backpack.data().upgrades;
    for (int i = 0; i < static_cast<int>(slots.size()); ++i) {
        auto const& s = slots[static_cast<std::size_t>(i)];
        if (i == skipSlot || s.empty() || !s.enabled) continue;
        if (auto const* t = tierByItem(s.item.id)) ms.push_back(multiplierOf(*t));
    }
    if (extra != 1.0) ms.push_back(extra);
    return ms;
}
} // namespace

double totalMultiplier(game::Backpack const& backpack, int skipSlot, double extra) {
    return core::stack::combine(multipliersOf(backpack, skipSlot, extra));
}

int slotLimit(game::Backpack const& backpack, int skipSlot, double extra) {
    return core::stack::slotLimitOf(multipliersOf(backpack, skipSlot, extra));
}

int minimumMultiplierRequired(std::shared_ptr<game::Backpack> const& backpack, int slot) {
    if (!backpack) return 0;
    UpgradeContext ctx(backpack, slot);
    // TankUpgradeWrapper / BatteryUpgradeWrapper.getMinimumMultiplierRequired:
    // ceil(contents / base capacity), the base capacity being the capacity at multiplier 1.
    // Handlers report the capacity at the current (adjusted) multiplier: divide it back out.
    auto tank   = api::backpack::tankOf(ctx);
    auto energy = tank ? nullptr : api::backpack::energyOf(ctx);
    if (!tank && !energy) return 0;
    double current  = core::capacity::adjustedMultiplier(api::backpack::stackMultiplier(*backpack));
    auto   required = [&](long long amount, long long capacity) {
        if (amount <= 0 || capacity <= 0) return 0;
        double base = static_cast<double>(capacity) / std::max(current, 1e-9);
        if (base <= 0.0) return 0;
        return static_cast<int>(std::min(std::ceil(static_cast<float>(amount) / base), static_cast<double>(INT_MAX)));
    };
    int need = 0;
    if (tank) {
        for (int i = 0; i < tank->tanks(); ++i) need = std::max(need, required(tank->fluidInTank(i).amount, tank->tankCapacity(i)));
    }
    if (energy) need = std::max(need, required(energy->stored(), energy->capacity()));
    return need;
}

/// The error sets of the last refused `fits` on this thread (StackUpgrade::refusal*Slots are asked
/// right after the refusing check).
struct Refusal {
    game::Backpack const* backpack = nullptr; ///< the refused backpack (nullptr = no refusal recorded)
    std::vector<int>      upgrades;
    std::vector<int>      storage;
};
thread_local Refusal tLastRefusal;

bool fits(
    std::shared_ptr<game::Backpack> const& backpack,
    double                                 multiplier,
    Player*                                player,
    std::string&                           reason,
    std::vector<int>*                      errorInventoryParts
) {
    // The mod compares against the multiplier itself: storage slots holding more than one item
    // whose count / max stack exceeds it, and every installed stackable-contents upgrade.
    bool             slotsOver = core::stack::firstSlotNeedingMore(backpack->inventory(), multiplier) >= 0;
    std::vector<int> partsOver;
    auto const&      slots = backpack->data().upgrades;
    for (int i = 0; i < static_cast<int>(slots.size()); ++i) {
        if (slots[static_cast<std::size_t>(i)].empty()) continue;
        if (minimumMultiplierRequired(backpack, i) > multiplier) partsOver.push_back(i);
    }
    // UpgradeSlotChangeResult.fail(message, {}, slotsOverMultiplier, errorInventoryParts), kept for
    // the native screen's red overlay.
    tLastRefusal          = {};
    tLastRefusal.backpack = backpack.get();
    if (slotsOver) {
        auto const& inv = backpack->inventory();
        for (int i = 0; i < inv.size(); ++i) {
            auto const& st = inv.at(i);
            if (st.count <= 1 || st.maxStack <= 0) continue;
            if (static_cast<double>(st.count) / static_cast<double>(st.maxStack) > multiplier) tLastRefusal.storage.push_back(i);
        }
    }
    tLastRefusal.upgrades = partsOver;
    if (!slotsOver && partsOver.empty()) {
        tLastRefusal = {};
        return true;
    }
    reason = msg(
        player,
        "abp.stack.error.low_multiplier",
        "There's at least one slot or upgrade that requires multiplier higher than {}",
        javaDouble(multiplier)
    );
    if (errorInventoryParts) *errorInventoryParts = std::move(partsOver);
    return false;
}

std::string formatMultiplier(double m) {
    auto num = [](double v) {
        if (v >= 1e6 || std::floor(v) == v) return std::format("{:.0f}", v);
        return std::format("{:g}", v);
    };
    if (m < 1.0 && m > 0.0) return "÷" + num(1.0 / m);
    return "×" + num(m);
}

/// One Stack upgrade / downgrade tier.
class StackUpgrade : public IUpgrade {
public:
    explicit StackUpgrade(TierDef const& def) : mDef(def) {
        mInfo.id           = std::string(def.id);
        mInfo.itemId       = itemIdOf(def.id);
        mInfo.nameKey      = "abp.stack." + std::string(def.id) + ".name";
        mInfo.nameFallback = std::string(def.fallback);
        mInfo.group        = std::string(kGroup);
        mInfo.toggleable   = false; // StackUpgradeItem.Wrapper.canBeDisabled() == false
        mInfo.hasSettings  = false;
        // The group "stack_upgrades": its limit (3) is the configurable
        // maxUpgradesPerStorage entry, checked by the admin unit.
        mInfo.maxPerBackpack = kUnlimitedPerBackpack;
    }

    [[nodiscard]] UpgradeInfo const& info() const override {
        return mInfo;
    }

    [[nodiscard]] double multiplier() const { return multiplierOf(mDef); }

    // StackUpgradeItem: the checks below project the multiplier after the change.
    bool canInstall(UpgradeContext const& ctx, std::string& reason) const override {
        // canAddUpgradeTo: current slot limit / 64 x this multiplier (the target slot is empty here).
        double whenAdded = static_cast<double>(slotLimit(ctx.backpack())) / 64.0 * multiplier();
        return fits(ctx.backpackPtr(), whenAdded, ctx.player(), reason);
    }

    bool canRemove(UpgradeContext const& ctx, std::string& reason) const override {
        // canRemoveUpgradeFrom: the slot limit without this upgrade.
        double whenRemoved = static_cast<double>(slotLimit(ctx.backpack(), ctx.slot())) / 64.0;
        return fits(ctx.backpackPtr(), whenRemoved, ctx.player(), reason);
    }

    bool canReplaceWith(UpgradeContext const& ctx, core::ItemStackData const& replacement, std::string& reason) const override {
        // canSwapUpgradeFor: another kind of upgrade counts as a removal; a stack upgrade with the
        // same or a higher multiplier is always allowed; a lower one must fit.
        auto const* other = tierByItem(replacement.id);
        if (!other) return canRemove(ctx, reason);
        double next = multiplierOf(*other);
        if (next >= multiplier()) return true;
        double whenRemoved = static_cast<double>(slotLimit(ctx.backpack(), ctx.slot())) / 64.0;
        return fits(ctx.backpackPtr(), whenRemoved * next, ctx.player(), reason);
    }

    // The error sets (slotsOverMultiplier, errorInventoryParts) of the
    // UpgradeSlotChangeResult of the check that just refused.
    // (Only when that check was for this backpack: a refusal from elsewhere, e.g. another
    // upgrade's allowsInstallOf, keeps the default overlay.)
    [[nodiscard]] std::vector<int> refusalStorageSlots(UpgradeContext const& ctx) const override {
        return tLastRefusal.backpack == &ctx.backpack() ? tLastRefusal.storage : std::vector<int>{};
    }
    [[nodiscard]] std::optional<std::vector<int>> refusalUpgradeSlots(UpgradeContext const& ctx) const override {
        if (tLastRefusal.backpack != &ctx.backpack()) return std::nullopt;
        return tLastRefusal.upgrades;
    }

    void attach(UpgradeContext const& ctx, core::BackpackInventory& inventory) override {
        // All stack upgrades together make one modifier (the product), registered by the first one.
        auto const& slots = ctx.backpack().data().upgrades;
        for (int i = 0; i < ctx.slot(); ++i) {
            auto const& s = slots[static_cast<std::size_t>(i)];
            if (!s.empty() && s.enabled && tierByItem(s.item.id)) return;
        }
        // The product is cached and recomputed after `/backpack reload` (config multipliers).
        struct Cached {
            std::uint64_t generation = 0;
            int           limit      = 64;
        };
        auto cached = std::make_shared<Cached>(Cached{gGeneration, slotLimit(ctx.backpack())});
        auto weak   = std::weak_ptr<game::Backpack>(ctx.backpackPtr());
        inventory.addLimitModifier(ctx.hookOwner(), 0, [cached, weak](int, core::ItemStackData const& item, int current) {
            if (cached->generation != gGeneration) {
                if (auto bp = weak.lock()) cached->limit = slotLimit(*bp);
                cached->generation = gGeneration;
            }
            // InventoryHandler.getBaseStackLimit; non-stackable items keep their own size.
            return affects(item) ? core::stack::stackLimit(current, cached->limit) : current;
        });
    }

    [[nodiscard]] std::vector<std::string> describe(Player& player, UpgradeContext const& ctx) const override {
        return {
            trFor(player, "abp.stack.describe.multiplier", "§7Stack size multiplier: §f{}", formatMultiplier(multiplier())),
            trFor(player, "abp.stack.describe.total", "§7Slot limit (all stack upgrades): §f{}", std::to_string(slotLimit(ctx.backpack()))),
        };
    }

private:
    TierDef const&      mDef;
    mutable UpgradeInfo mInfo;
};

namespace {
template <std::size_t I>
struct TierUpgrade : StackUpgrade {
    TierUpgrade() : StackUpgrade(kTiers[I]) {}
};
} // namespace

} // namespace abp::upgrade::stack

ABP_REGISTER_UPGRADE(abp::upgrade::stack::TierUpgrade<0>);
ABP_REGISTER_UPGRADE(abp::upgrade::stack::TierUpgrade<1>);
ABP_REGISTER_UPGRADE(abp::upgrade::stack::TierUpgrade<2>);
ABP_REGISTER_UPGRADE(abp::upgrade::stack::TierUpgrade<3>);
ABP_REGISTER_UPGRADE(abp::upgrade::stack::TierUpgrade<4>);
ABP_REGISTER_UPGRADE(abp::upgrade::stack::TierUpgrade<5>);
ABP_REGISTER_UPGRADE(abp::upgrade::stack::TierUpgrade<6>);
ABP_REGISTER_UPGRADE(abp::upgrade::stack::TierUpgrade<7>);
ABP_REGISTER_UPGRADE(abp::upgrade::stack::TierUpgrade<8>);
ABP_REGISTER_MODULE(abp::upgrade::stack::StackModule);
