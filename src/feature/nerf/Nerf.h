#pragma once

// The nerfs (see Nerf.cpp).

class MobEffect;
class Player;

namespace abp::game {
struct CarriedBackpack;
}

namespace abp::nerf {

/// The effect applied for carrying too many backpacks (slowness).
[[nodiscard]] MobEffect const* slownessEffect();
/// Applies the too-many-backpacks effect to `player` now (if on and over the limit). Returns the
/// number of levels applied (0 = none).
int applySlowness(Player& player);
/// `onlyWornBackpackTriggersUpgrades`: whether a carried backpack's upgrades run.
[[nodiscard]] bool upgradesActive(game::CarriedBackpack const& carried);

} // namespace abp::nerf
