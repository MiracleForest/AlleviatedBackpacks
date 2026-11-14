#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "mc/world/level/BlockPos.h"

#include "api/abp/Fluid.h"
#include "upgrade/IUpgrade.h"

class Player;
class BlockSource;

namespace abp::upgrade::pump {

/// Pump upgrades ("Pump Upgrade" / "Advanced Pump Upgrade",
/// `PumpUpgradeWrapper`). Each tick (after its cooldown) they move fluids between the backpack's
/// tanks and the first of these that works:
/// - the fluid container in the carrier's main hand, then offhand, then in the hands of players
///   within 3 blocks ("hand"); cooldown 3 ticks;
/// - fluid source blocks within 4 blocks (input) or the blocks around the backpack (output, not
///   above) ("world");
/// - adjacent tanks and pipes, always, `maxInputOutput` x rows x multiplier (at least a bucket) per
///   operation.
/// Defaults (the items): basic = hand off, world on; advanced = hand on, world on. Only the
/// advanced tab can change hand / world. Both pump into the backpack ("input") by default. The advanced pump has a 4 slot fluid filter.
inline constexpr std::string_view kBasicId    = "pump";
inline constexpr std::string_view kAdvancedId = "advanced_pump";

struct Settings {
    bool                     input    = true;
    bool                     hand     = false;
    bool                     world    = true;
    std::vector<std::string> filter; ///< phase-1 fluid ids ("" = empty slot), `filterSlots` long
    bool                     advanced = false;

    [[nodiscard]] static Settings of(UpgradeContext const& ctx);
    void                          save(UpgradeContext const& ctx) const;
    /// FluidFilterLogic.fluidMatches: an empty filter matches everything.
    [[nodiscard]] bool matches(api::FluidStack const& fluid) const;
};

/// Number of fluid filter slots (`config/pump.json` filterSlots, the default 4).
[[nodiscard]] int filterSlots();

/// One pump tick (PumpUpgradeWrapper.tick) for a carried backpack (`carrier` set) or a placed one (`region` + `pos`).
/// Returns the cooldown it chose (ticks).
int pumpTick(UpgradeContext const& ctx, Player* carrier, game::ItemLocation const* backpackItem, BlockSource& region, BlockPos const& pos);

/// NeoForge FluidUtil.tryFluidTransfer: moves up to `resource.amount` of that fluid from `source` to
/// `dest`. Returns what moved.
api::FluidStack tryFluidTransfer(api::IFluidHandler& dest, api::IFluidHandler& source, api::FluidStack const& resource);

} // namespace abp::upgrade::pump
