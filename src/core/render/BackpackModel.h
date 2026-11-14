#pragma once

// Render state of a backpack model (P2-11), the Bedrock counterpart of the `RenderInfo`
// tank / battery / item display parts. Built on the server, sent to companion clients
// inside `screen::RenderInfo::extra["model"]`, read by the client renderers. Pure (no game types).

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "core/ItemStackData.h"

namespace abp::core::render {

/// BackpackModel draws the tank fluid in 10 heights (`renderFluid`: `(int)(fill * 10)`)
/// and the battery charge in 4 (`renderBatteryCharge`: `(int)(chargeRatio * 4)`).
inline constexpr int kFluidSteps  = 10;
inline constexpr int kChargeSteps = 4;

/// The defaults (BackpackWrapper.DEFAULT_MAIN_COLOR / DEFAULT_ACCENT_COLOR), ARGB.
inline constexpr std::uint32_t kDefaultMainColor   = 0xFFCC613A;
inline constexpr std::uint32_t kDefaultAccentColor = 0xFF622E1A;

/// Upgrade particles on worn backpacks: 1 in 32 frames (BackpackLayerRenderer.renderUpgrades).
inline constexpr int kUpgradeParticleChance = 32;

/// Fluid look: which fluid texture the tank uses (our own textures, not the fluid's
/// still texture).
enum class FluidLook : int { Water = 0, Lava = 1, Milk = 2, PowderSnow = 3, Honey = 4, Experience = 5, Potion = 6 };

struct TankModel {
    std::string   fluid;           ///< fluid id (core ids: "water", "lava", "potion:5", ...)
    float         ratio   = 0;     ///< fill ratio 0..1
    std::uint32_t color   = 0xFFFFFFFF; ///< ARGB tint of the fluid texture (fills from the bottom)

    [[nodiscard]] bool operator==(TankModel const&) const = default;
};

struct DisplayModel {
    ItemStackData item;          ///< the displayed item (count 1)
    int           rotation = 0;  ///< degrees (ItemDisplaySettingsCategory: steps of 45)
    std::string   side     = "front"; ///< DisplaySide: "front" | "left" | "right"

    [[nodiscard]] bool operator==(DisplayModel const&) const = default;
};

struct BackpackModel {
    std::optional<TankModel>    left;
    std::optional<TankModel>    right;
    std::optional<float>        battery; ///< charge ratio when a battery upgrade is installed
    std::optional<DisplayModel> display;
    /// Ids of upgrades that currently show activity particles ("cooking" burning, "jukebox"
    /// playing), the upgrade render data.
    std::vector<std::string>    activity;

    [[nodiscard]] bool           operator==(BackpackModel const&) const = default;
    [[nodiscard]] nlohmann::json toJson() const;
    [[nodiscard]] static BackpackModel fromJson(nlohmann::json const& j);
};

enum class TankSide { Left, Right };

/// UpgradeHandler.initTankRenderInfoCallbacks: the first tank goes LEFT and every later one
/// RIGHT; a single tank goes RIGHT when its upgrade slot is in the second half (slot >= slots/2).
/// `tankSlots` are the upgrade-slot indexes of the tank upgrades in slot order.
[[nodiscard]] std::vector<TankSide> tankSides(std::vector<int> const& tankSlots, int upgradeSlots);

/// The step shown for a fill / charge ratio, as BackpackModel picks it: `(int)(ratio * steps)`,
/// with the ratio clamped to 0..1.
[[nodiscard]] int ratioToStep(float ratio, int steps);

/// Fluid texture for a fluid id.
[[nodiscard]] FluidLook fluidLook(std::string_view fluid);

} // namespace abp::core::render
