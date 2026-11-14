#pragma once

#include <functional>
#include <string>

#include "nlohmann/json.hpp"

#include "core/Filter.h"
#include "upgrade/filter/FilterTab.h"
#include "upgrade/IUpgrade.h"

class Player;

namespace abp::upgrade::magnet {

/// Magnet upgrade (basic / advanced), behaviour of `MagnetUpgradeWrapper`:
///
/// - Items: item entities inside the cube `AABB(pos).inflate(magnetRange)` around the carrier's
///   (or the placed backpack's) block position that are alive, pass the contents filter and carry
///   neither the `PreventRemoteMovement` tag (nor, for a placed backpack, `PreventRemoteMovement`
///   without `AllowMachineRemoteMovement`) are inserted into the backpack, whatever their pickup
///   delay (so items the carrier just threw are pulled as well). Walking over items works like a
///   Pickup upgrade with the same filter.
/// - XP: only when the backpack's fluid handler (tank upgrades, via the capability API) accepts
///   liquid experience: every orb in range is turned into 20 mB per point; whatever does not fit
///   is spawned again as an orb.
/// - Timing: every installed magnet (carried or placed) has its own cooldown. The item pass gives
///   40 ticks when an eligible item could not be inserted (or item pickup is off), else 10; the XP
///   pass gives 40 when an orb could not be stored, else 10; the shorter of the two is used.
///
/// Settings JSON (`ctx.settings()`): {"filter": core::Filter, "pickupItems": bool, "pickupXp": bool}.

inline constexpr std::string_view kBasicId    = "magnet";
inline constexpr std::string_view kAdvancedId = "advanced_magnet";

inline constexpr int kCooldownTicks     = 10; ///< COOLDOWN_TICKS
inline constexpr int kFullCooldownTicks = 40; ///< FULL_COOLDOWN_TICKS

/// Entity tags that keep a magnet away from an item entity / orb (`PreventRemoteMovement`,
/// `AllowMachineRemoteMovement`; Bedrock entity tags, e.g. `/tag @e add PreventRemoteMovement`).
inline constexpr char const* kPreventRemoteMovement = "PreventRemoteMovement";
inline constexpr char const* kAllowMachineMovement  = "AllowMachineRemoteMovement";

/// `MagnetUpgradeConfig` of one tier.
struct MagnetTierConfig {
    int filterSlots = 9; ///< Number of filter slots (1..20)
    int slotsInRow  = 3; ///< Number of filter slots displayed in a row (1..6)
    int magnetRange = 3; ///< Range around storage in blocks at which magnet will pickup items (1..20)
};

/// `config/magnet.json` (the server config sections `magnetUpgrade` / `advancedMagnetUpgrade`).
struct MagnetConfig {
    int              version               = 2;
    MagnetTierConfig magnetUpgrade         = {9, 3, 3};
    MagnetTierConfig advancedMagnetUpgrade = {16, 4, 5};
};

/// The (clamped) config of a tier.
[[nodiscard]] MagnetTierConfig tierConfig(bool advanced);

/// Decoded per-upgrade settings.
struct Settings {
    bool         pickupItems = true;
    bool         pickupXp    = true;
    core::Filter filter;

    [[nodiscard]] static Settings read(nlohmann::json const& settings, int filterSlots, bool advanced);
};

/// The shared filter control of a magnet (ContentsFilterControl: Allow / Block / Match
/// Backpack Contents).
[[nodiscard]] filtering::FilterSpec filterSpec(bool advanced);

/// `ContentsFilterLogic.matchesFilter`: the filter, or with "Match Backpack Contents" the
/// stored stacks and memorized slots.
[[nodiscard]] bool matchesFilter(Settings const& s, game::Backpack& backpack, core::ItemStackData const& item);

/// Something that can take XP collected by a magnet. Kept for other units (XP Pump); the magnet
/// itself now fills the backpack's tanks.
using XpConsumer = std::function<int(UpgradeContext const& magnet, Player* carrier, int amount)>;

/// Registers an XP consumer under `owner` (lower `priority` asks first).
void addXpConsumer(std::string owner, int priority, XpConsumer fn);
/// Removes every consumer registered under `owner`.
void removeXpConsumers(std::string const& owner);
/// Offers `amount` XP to the consumers; returns what is left.
int offerXp(UpgradeContext const& magnet, Player* carrier, int amount);

} // namespace abp::upgrade::magnet
