#pragma once

#include "core/BackpackInventory.h"
#include "core/Filter.h"
#include "upgrade/filter/FilterTab.h"
#include "upgrade/IUpgrade.h"
#include "upgrade/void/VoidRules.h"

namespace abp::upgrade::voiding {

/// Upgrade ids / items.
inline constexpr std::string_view kBasicId    = "void";
inline constexpr std::string_view kAdvancedId = "advanced_void";

/// Settings keys in the upgrade's settings JSON.
inline constexpr std::string_view kFilterKey       = "filter";
inline constexpr std::string_view kVoidOverflowKey = "voidOverflow"; ///< SHOULD_VOID_OVERFLOW (default false)
inline constexpr std::string_view kWorkInGuiKey    = "workInGui";    ///< SHOULD_WORK_IN_GUI (default false)

/// `VoidUpgradeConfig` of one tier.
struct VoidTierConfig {
    int  filterSlots         = 9;    ///< Number of filter slots (1..20)
    int  slotsInRow          = 3;    ///< Number of filter slots displayed in a row (1..6)
    bool voidAnythingEnabled = true; ///< false: only the "Void Overflow" mode is offered
};

/// `config/void.json` (the server config sections `voidUpgrade` / `advancedVoidUpgrade`).
struct VoidConfig {
    int            version             = 2;
    VoidTierConfig voidUpgrade         = {9, 3, true};
    VoidTierConfig advancedVoidUpgrade = {16, 4, true};
};

[[nodiscard]] VoidTierConfig tierConfig(bool advanced);
/// The shared filter control (`FilterLogicControl.Basic` / `.Advanced`: Allow / Block,
/// advanced match options).
[[nodiscard]] filtering::FilterSpec filterSpec(bool advanced);
/// FilterLogic with `setAllowByDefault(true)`: a new filter is an (empty) allow list.
[[nodiscard]] core::Filter readFilter(nlohmann::json const& settings, bool advanced);

/// Runs after the foundation rules (priority 1000) and after other upgrades' filters, so a Deny
/// from them (e.g. a Filter upgrade) wins over voiding.
inline constexpr int kInsertPriority = 1001;

/// Parsed settings of one installed Void upgrade.
struct VoidSettings {
    core::Filter filter;
    bool         advanced            = false;
    bool         storedOverflow      = false; ///< as stored
    bool         voidAnythingEnabled = true;
    bool         workInGui           = false;

    /// `shouldVoidOverflow`: the overflow mode is forced when "void anything" is disabled.
    [[nodiscard]] bool shouldVoidOverflow() const { return !voidAnythingEnabled || storedOverflow; }
    /// `filterLogic.matchesFilter` (allow list by default; an empty allow list matches
    /// nothing, an empty block list everything).
    [[nodiscard]] bool      matches(core::ItemStackData const& item) const;
    [[nodiscard]] VoidRules rules() const;

    [[nodiscard]] static VoidSettings read(UpgradeContext const& ctx);
};

} // namespace abp::upgrade::voiding
