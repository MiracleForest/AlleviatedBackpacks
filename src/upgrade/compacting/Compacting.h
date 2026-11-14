#pragma once

// Compacting upgrade (basic: 2x2; advanced: 3x3 then 2x2)
// CompactingUpgradeWrapper. See Compacting.cpp.

#include <optional>
#include <string_view>
#include <vector>

#include "core/Filter.h"
#include "upgrade/filter/FilterTab.h"
#include "upgrade/IUpgrade.h"
#include "upgrade/compacting/CompactingRecipes.h"

namespace abp::upgrade::compacting {

inline constexpr std::string_view kBasicId    = "compacting";
inline constexpr std::string_view kAdvancedId = "advanced_compacting";

/// Settings keys.
inline constexpr std::string_view kFilterKey           = "filter";
inline constexpr std::string_view kCompactAnythingKey  = "compactNonUncraftable"; ///< COMPACT_NON_UNCRAFTABLE (default false)
inline constexpr std::string_view kWorkInGuiKey        = "workInGui";             ///< SHOULD_WORK_IN_GUI (default false)

/// Config `compactingUpgrade` (a FilteredUpgradeConfig: 9 slots, 3 per row).
struct CompactingBasicConfig {
    int filterSlots = 9; ///< Number of filter slots (1..20)
    int slotsInRow  = 3; ///< Number of filter slots displayed in a row (1..6)
};
/// Config `advancedCompactingUpgrade` (a FilteredUpgradeConfig: 16 slots, 4 per row).
struct CompactingAdvancedConfig {
    int filterSlots = 16;
    int slotsInRow  = 4;
};

/// `config/compacting.json`. Version 3 dropped the extra-shape lists.
struct CompactingConfig {
    int                      version                   = 3;
    CompactingBasicConfig    compactingUpgrade         = {};
    CompactingAdvancedConfig advancedCompactingUpgrade = {};
};

[[nodiscard]] int filterSlots(bool advanced);
[[nodiscard]] int slotsInRow(bool advanced);
/// The shared filter control (FilterLogicControl: Allow / Block).
[[nodiscard]] filtering::FilterSpec filterSpec(bool advanced);

/// Settings of one installed compacting upgrade.
struct CompactingSettings {
    core::Filter filter;
    bool         advanced        = false;
    bool         compactAnything = false; ///< "Compact Anything" (else only uncraftable)
    bool         workInGui       = false;

    [[nodiscard]] static CompactingSettings read(nlohmann::json const& settings, bool advanced);
};

/// What one compacting craft of an item makes (RecipeHelper.CompactingResult) and how many
/// items it takes (width * height of the grid used).
struct CompactingCraft {
    core::ItemStackData              result;
    int                              count = 0;
    std::vector<core::ItemStackData> remaining; ///< CompactingResult.getRemainingItems (GX-2)
};

/// The grid choice of `CompactingUpgradeWrapper.compactSlot`: 3x3 (advanced only) when
/// that shape qualifies, else 2x2 when it qualifies; "qualifies" = uncraftable, or any recipe with
/// "Compact Anything". Nothing otherwise.
[[nodiscard]] std::optional<CompactingCraft> compactingCraft(core::ItemStackData const& item, bool advanced, bool compactAnything);

/// The filter validity: the item has any 2x2 / 3x3 compacting shape (whatever the tier and
/// the "Compact Anything" setting).
[[nodiscard]] bool canCompact(core::ItemStackData const& item);

} // namespace abp::upgrade::compacting
