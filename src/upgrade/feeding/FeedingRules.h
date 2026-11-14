#pragma once

// Pure decision rules of the Feeding upgrade (no game headers; unit-tested in tests/), as
// FeedingUpgradeWrapper / HungerLevel.

#include <algorithm>
#include <string>
#include <string_view>

namespace abp::upgrade::feeding {

/// `HungerLevel`: when food gets fed to the player.
enum class HungerLevel : int {
    Any  = 0, ///< as soon as the player is a tiny bit hungry
    Half = 1, ///< when at most half of the food's hunger points would be wasted (default)
    Full = 2, ///< when none of the food's hunger points would be wasted
};

[[nodiscard]] constexpr std::string_view toString(HungerLevel l) {
    switch (l) {
    case HungerLevel::Any:
        return "any";
    case HungerLevel::Full:
        return "full";
    case HungerLevel::Half:
    default:
        return "half";
    }
}
/// `HungerLevel.fromName` (unknown names: HALF).
[[nodiscard]] constexpr HungerLevel hungerLevelFromName(std::string_view name) {
    if (name == "any") return HungerLevel::Any;
    if (name == "full") return HungerLevel::Full;
    return HungerLevel::Half;
}
/// `HungerLevel.next`: ANY -> HALF -> FULL -> ANY.
[[nodiscard]] constexpr HungerLevel next(HungerLevel l) { return static_cast<HungerLevel>((static_cast<int>(l) + 1) % 3); }

struct FeedingOptions {
    HungerLevel level                    = HungerLevel::Half; ///< the default HALF
    bool        feedImmediatelyWhenHurt = true;              ///< the default true
};

/// The player's state relevant to feeding.
struct EaterState {
    int   foodLevel = 20; ///< 0..20
    float health    = 20.f;
    float maxHealth = 20.f;

    /// `hungerLevel = 20 - foodLevel`.
    [[nodiscard]] int hunger() const { return std::max(0, 20 - foodLevel); }
    /// `player.getHealth() < player.getMaxHealth() - 0.1F`.
    [[nodiscard]] bool hurt() const { return health < maxHealth - 0.1f; }
};

/// `isEdible`: an item with a food component and nutrition >= 1. Java's ominous bottle
/// has no food component (so it is never fed); Bedrock's is excluded by id to match.
[[nodiscard]] inline bool isEdible(std::string_view id, int nutrition) {
    return id != "minecraft:ominous_bottle" && nutrition >= 1;
}

/// `isHungryEnoughForFood`.
[[nodiscard]] inline bool isHungryEnoughForFood(int hunger, int nutrition, HungerLevel level) {
    if (level == HungerLevel::Any) return true;
    return (level == HungerLevel::Half ? nutrition / 2 : nutrition) <= hunger;
}

/// `tryFeedingStack` condition (edibility and filter are checked separately): whether a food
/// worth `nutrition` should be fed now. Never when the player isn't hungry at all.
[[nodiscard]] inline bool shouldFeed(EaterState const& s, int nutrition, FeedingOptions const& o) {
    int hunger = s.hunger();
    if (hunger == 0) return false;
    return isHungryEnoughForFood(hunger, nutrition, o.level) || (o.feedImmediatelyWhenHurt && hunger > 0 && s.hurt());
}

} // namespace abp::upgrade::feeding
