#pragma once

// Pure rules of the world systems (P2-13; EntityBackpackAdditionHandler, CommonEventHandler,
// ListCommand), shared by the game adapters and the
// unit tests: entity backpack additions (local difficulty, tier pools, drop chance), the
// too-many-backpacks nerf, and the `/backpackadmin list` presentation (colour swatches, date format).

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/Tier.h"

namespace abp::core {

// ---- local difficulty (vanilla DifficultyInstance) ------------------------------------------------

/// Moon brightness of a moon phase (0..7): 1, .75, .5, .25, 0, .25, .5, .75.
[[nodiscard]] float moonBrightness(int phase);

/// Effective local difficulty (0..6.75): `difficulty` 0 peaceful .. 3 hard, `levelTime` world age in
/// ticks, `inhabitedTime` ticks players spent in the chunk, `moon` moon brightness.
[[nodiscard]] float effectiveDifficulty(int difficulty, std::int64_t levelTime, std::int64_t inhabitedTime, float moon);
/// Vanilla's special multiplier (0..1) for an effective difficulty.
[[nodiscard]] float specialMultiplier(float effectiveDifficulty);

// ---- entity backpack additions ----------------------------------------------------------------------

/// EntityBackpackAdditionHandler: floor(buckets / 6.75 * effective - 0.1), clamped to
/// [0, buckets - 1] (3 buckets: low, mid and high local difficulty).
[[nodiscard]] int difficultyIndex(float effectiveDifficulty, int buckets = 3);

/// The weighted tiers a mob can get at a difficulty index (BACKPACK_CHANCES /
/// DIFFICULTY_BACKPACK_CHANCES, fixed weights netherite 1, diamond 5, gold 25, iron 125,
/// copper 250, leather 625): index 0 all six tiers, 1 diamond..copper, 2 gold..copper.
struct TierChoice {
    Tier tier;
    int  weight;
};
[[nodiscard]] std::vector<TierChoice> tierPool(int index);
/// Weighted pick with `roll` in [0, total weight). Empty pool or total 0: Leather.
[[nodiscard]] Tier pickWeighted(std::vector<TierChoice> const& pool, int roll);
[[nodiscard]] int  totalWeight(std::vector<TierChoice> const& pool);

/// The per-tier "min difficulty" (0 leather, 1 copper/iron, 2 gold, 3 diamond, 4 netherite): loot
/// rolls, potion amplifiers, health bonus and armor all scale with it.
[[nodiscard]] int minDifficultyOf(Tier tier);
/// The drop chance multiplier per tier: 1 / 1.25 / 1.5 / 3 / 4.5 / 6.
[[nodiscard]] double dropChanceMultiplier(Tier tier);
/// (base + looting * perLevel) * multiplier(tier).
[[nodiscard]] double dropChance(double base, double perLootingLevel, int lootingLevel, Tier tier);

/// Armor material weights per tier: index 0 netherite, 1 diamond, 2 gold, 3 iron, 4 leather
/// with weights 1, 3, 9, 27, 81; each tier draws helmet, leggings and boots from a sub-range.
struct ArmorRange {
    int first; ///< first material index
    int last;  ///< one past the last
};
struct ArmorRanges {
    ArmorRange helmet, leggings, boots;
};
[[nodiscard]] ArmorRanges armorRanges(Tier tier);
[[nodiscard]] int         armorWeight(int material);

// ---- nerfs -------------------------------------------------------------------------------------------

/// Slowness levels for carrying `count` backpacks: min(10, ceil((count - max) * ratio)), 0 when
/// `count <= max`.
[[nodiscard]] int slownessLevels(int count, int maxBackpacks, double levelsPerAdditional);

// ---- /backpackadmin list presentation -------------------------------------------------------------

/// The Bedrock formatting code (the character after '§') whose colour is nearest to `rgb`
/// (0xRRGGBB; alpha ignored).
[[nodiscard]] char nearestFormatCode(std::uint32_t rgb);
/// Java's `new SimpleDateFormat()` in en_US: "M/d/yy, h:mm AM".
[[nodiscard]] std::string shortDateTime(int year, int month, int day, int hour, int minute);

} // namespace abp::core
