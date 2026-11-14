#pragma once

#include <vector>

namespace abp::core::columns {

/// Whether a backpack inventory can shrink to `targetSlots` when upgrade inventory parts (Tank,
/// Battery) take storage columns. Items never move: the slots that disappear (every slot at or
/// above `targetSlots`) must all be empty (`StorageContainerMenuBase.needsSlotsThatAreOccupied`);
/// the inventory is then cut to the new size (`InventoryHandler.changeSlots`).
struct FitResult {
    bool             fits = false;
    std::vector<int> errorSlots; ///< occupied slots at or above `targetSlots` (ascending; empty when `fits`)
};

/// `occupied[i]`: slot i holds an item.
[[nodiscard]] FitResult fit(std::vector<bool> const& occupied, int targetSlots);

} // namespace abp::core::columns
