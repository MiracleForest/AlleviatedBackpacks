#pragma once

// Smelting / Smoking / Blasting upgrades and their Auto variants (
// `upgrades/cooking`).
//
// Each upgrade owns a small furnace (input 0, fuel 1, output 2) kept in its settings JSON, so the
// contents and the cooking state travel with the upgrade item. The furnace runs while the backpack
// is carried or placed, on the game's furnace / smoker / blast furnace recipes and fuel values,
// with the per-upgrade speed and fuel efficiency multipliers. The Auto variants push
// results into the backpack and pull fuel and inputs (through the input and fuel filters).
// Smelting in an upgrade awards no experience. One furnace upgrade per backpack
// (the group "furnace_upgrades", shown as "Furnace Upgrades"). Disabling the upgrade pauses
// it. The native tab is client/ui/tabs/cooking.

#include <optional>
#include <string_view>

#include "core/Filter.h"
#include "upgrade/IUpgrade.h"
#include "upgrade/cooking/CookingLogic.h"

class Player;

namespace abp::upgrade::cooking {

enum class Kind { Smelting, Smoking, Blasting };

/// CookingUpgradeConfig (per upgrade; the server config).
struct CookingUpgradeConfig {
    /// Smelting speed multiplier (1.0 equals speed at which vanilla furnace smelts items), 0.25..4.
    double smeltingSpeedMultiplier  = 1.0;
    /// Fuel efficiency multiplier (1.0 equals speed at which it's used in vanilla furnace), 0.25..4.
    double fuelEfficiencyMultiplier = 1.0;
};

/// AutoCookingUpgradeConfig (per Auto upgrade; the server config).
struct AutoCookingUpgradeConfig {
    double smeltingSpeedMultiplier  = 1.0;
    double fuelEfficiencyMultiplier = 1.0;
    /// Number of input filter slots (1..20).
    int    inputFilterSlots         = 8;
    /// Number of input filter slots displayed in a row (1..6).
    int    inputFilterSlotsInRow    = 4;
    /// Number of fuel filter slots (1..20).
    int    fuelFilterSlots          = 4;
    /// Number of fuel filter slots displayed in a row (1..6).
    int    fuelFilterSlotsInRow     = 4;
};

/// `config/cooking.json` (the server config sections of the six upgrades).
struct CookingConfig {
    int                      version = 2;
    CookingUpgradeConfig     smeltingUpgrade;
    CookingUpgradeConfig     smokingUpgrade;
    CookingUpgradeConfig     blastingUpgrade;
    AutoCookingUpgradeConfig autoSmeltingUpgrade;
    AutoCookingUpgradeConfig autoSmokingUpgrade;
    AutoCookingUpgradeConfig autoBlastingUpgrade;
};

/// The upgrade group of the six cooking upgrades (max one per backpack).
inline constexpr std::string_view kGroup = "furnace_upgrades";

/// Recipe tag of the vanilla block ("furnace", "smoker", "blast_furnace").
[[nodiscard]] std::string_view recipeTag(Kind kind);
/// Vanilla recipe cooking time (the mod reads it from the recipe): smelting 200, smoking / blasting 100 ticks.
[[nodiscard]] int recipeTime(Kind kind);
/// burnTimeModifier: 1 for smelting, 0.5 for smoking / blasting.
[[nodiscard]] float burnTimeModifier(Kind kind);

/// The result of cooking one `input` item in `kind`, or nullopt.
[[nodiscard]] std::optional<core::ItemStackData> recipeResult(Kind kind, core::ItemStackData const& input);
/// Furnace burn ticks of one `fuel` item (0 = not a fuel).
[[nodiscard]] int furnaceBurnTicks(core::ItemStackData const& fuel);

class CookingUpgrade : public IUpgrade {
public:
    CookingUpgrade(Kind kind, bool automatic);

    [[nodiscard]] UpgradeInfo const& info() const override { return mInfo; }
    [[nodiscard]] Kind               kind() const { return mKind; }
    [[nodiscard]] bool               automatic() const { return mAuto; }

    void onTick(TickContext const& tick) override;
    void onPlacedTick(PlacedTickContext const& tick) override;
    /// onBeforeRemoved -> pause: the burning fuel and the cooking progress are lost; the
    /// items stay with the upgrade item.
    void onRemove(UpgradeContext const& ctx) override;
    [[nodiscard]] std::vector<std::string> describe(Player& player, UpgradeContext const& ctx) const override;

    /// Speed and fuel multipliers of this upgrade (clamped to the 0.25..4).
    [[nodiscard]] double speedMultiplier() const;
    [[nodiscard]] double fuelMultiplier() const;
    /// Auto variants: filter slots and slots per row (clamped to the ranges).
    [[nodiscard]] int inputFilterSlots() const;
    [[nodiscard]] int inputFilterSlotsInRow() const;
    [[nodiscard]] int fuelFilterSlots() const;
    [[nodiscard]] int fuelFilterSlotsInRow() const;

    /// The game-backed environment for this upgrade at `ctx` (the Auto variants' filters included).
    [[nodiscard]] CookEnv env(UpgradeContext const& ctx) const;
    /// Runs the furnace for `ticks` game ticks. Returns true if the state changed.
    bool advance(UpgradeContext const& ctx, int ticks) const;

    // ---- state access (settings JSON) ---------------------------------------------------------
    [[nodiscard]] static CookState state(UpgradeContext const& ctx);
    static void                    setState(UpgradeContext const& ctx, CookState const& state);
    /// The input filter (a new filter is a block list; an empty allow list matches nothing).
    [[nodiscard]] core::Filter inputFilter(UpgradeContext const& ctx) const;
    /// The fuel filter (a new filter is an allow list; an empty allow list matches everything).
    [[nodiscard]] core::Filter fuelFilter(UpgradeContext const& ctx) const;
    static void                setInputFilter(UpgradeContext const& ctx, core::Filter const& filter);
    static void                setFuelFilter(UpgradeContext const& ctx, core::Filter const& filter);

    /// True while `ctx` still refers to an installed upgrade of this type.
    [[nodiscard]] bool installedAt(UpgradeContext const& ctx) const;

private:
    Kind        mKind;
    bool        mAuto;
    UpgradeInfo mInfo;
};

/// The registered upgrade for `kind` / `automatic`.
[[nodiscard]] CookingUpgrade& upgradeOf(Kind kind, bool automatic);
/// The cooking upgrade type installed at `ctx` (nullptr if none).
[[nodiscard]] CookingUpgrade* cookingAt(UpgradeContext const& ctx);

} // namespace abp::upgrade::cooking
