#pragma once

// Recipe side of the compacting upgrades: which full 2x2 / 3x3 crafting grids of one item give a
// result, and whether that result crafts back (RecipeHelper compacting part).

#include <optional>
#include <string>
#include <vector>

#include "core/ItemStackData.h"

namespace abp::upgrade::compacting {

/// A compacting recipe found in the game's crafting recipes: a full `gridSize` x `gridSize` grid
/// of one item produces `result` (with `result.count` items).
struct Compaction {
    core::ItemStackData result;             ///< count = items produced by one craft
    int                 inputs     = 0;     ///< items consumed by one craft (4 or 9)
    bool                reversible = false; ///< one result crafts back into exactly `inputs` items ("uncraftable")
    /// `RecipeHelper.CompactingResult.getRemainingItems` (Java `Recipe.getRemainingItems`):
    /// what one craft leaves behind, e.g. 4 glass bottles for a honey block. Bedrock lists them as
    /// the recipe's extra results; without those, each consumed item's `craftingRemainingItem`.
    std::vector<core::ItemStackData> remaining;
};

/// `RecipeHelper.getCompactingResult(stack, width, height)`: the crafting result of a
/// full `gridSize` x `gridSize` grid (2 or 3) of `item`; with several matching recipes the one
/// whose result crafts back into the item is preferred. Cached; the cache is dropped whenever the
/// game (re)loads its recipes or a new world starts. Items with custom data (names, enchantments,
/// ...) or damage never compact. Server thread only.
[[nodiscard]] std::optional<Compaction> findCompaction(core::ItemStackData const& item, int gridSize);

/// `RecipeHelper.uncompactMatchesItem`: one `result` crafts (1x1) back into exactly
/// `count` of `item`.
[[nodiscard]] bool uncompactMatches(core::ItemStackData const& result, core::ItemStackData const& item, int count);

/// `RecipeHelper.CompactingShape` classification of an item for full 2x2 / 3x3 grids.
struct CompactingShapes {
    bool twoByTwo                = false; ///< any 2x2 compaction (not the same item, not a loop)
    bool twoByTwoUncraftable     = false; ///< ... that crafts back into the 4 items
    bool threeByThree            = false;
    bool threeByThreeUncraftable = false;

    /// any shape other than NONE.
    [[nodiscard]] bool any() const { return twoByTwo || threeByThree; }
};
/// `RecipeHelper.getItemCompactingShapes` (compactions that are part of a compacting
/// loop are left out).
[[nodiscard]] CompactingShapes compactingShapes(core::ItemStackData const& item);

/// Drops the recipe cache (tests, recipe reload).
void invalidateCache();

} // namespace abp::upgrade::compacting
