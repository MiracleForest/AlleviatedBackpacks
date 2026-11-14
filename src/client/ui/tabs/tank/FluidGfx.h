#pragma once

// Drawing helpers shared by the Tank, Battery, Pump and Experience Pump screens: the fluid / energy
// bars (laid out like TankInventoryPart / BatteryInventoryPart, drawn procedurally) and
// the 16x16 button icons. All pixel art here is original.

#include <cstdint>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "client/ui/gfx/Painter.h"
#include "client/ui/widgets/Widgets.h"

class ItemStack;

namespace abp::client::ui::fluidgfx {

/// A fluid column `w` x `h` (where GuiHelper.renderTiledFluidTextureAtlas tiles the fluid's
/// still texture): an original "still" pattern tinted with the fluid colour, ARGB.
void fluid(Painter& p, float x, float y, float w, float h, std::uint32_t argb);
/// The same for a fluid of the server's tab data (`{"id", "nbt", "color"}`):
/// water, lava, milk and potions use the game's real still textures (the block atlas flipbooks,
/// animated and frame-blended like the atlas), tinted like their NeoForge fluid types (water's
/// tint, potion colour); other fluids fall back to the tinted pattern above.
void fluid(Painter& p, float x, float y, float w, float h, nlohmann::json const& fluid);

/// The bar background (GuiHelper.BAR_BACKGROUND_*): an 18 wide recessed column of height `h`.
void barBackground(Painter& p, float x, float y, float h);
/// The tank overlay (16x18 per row, as TankInventoryPart): level marks, `h / 18` rows from `y`.
void tankOverlay(Painter& p, float x, float y, float h);
/// The battery overlay (as BatteryInventoryPart): the empty charge cells.
void batteryCells(Painter& p, float x, float y, float h);
/// One 16x6 charge segment in `rgb`.
void chargeSegment(Painter& p, float x, float y, std::uint32_t rgb);
/// The 16x4 battery terminals above and below the bar.
void batteryConnectionTop(Painter& p, float x, float y);
void batteryConnectionBottom(Painter& p, float x, float y);

enum class Icon {
    PumpInput,   ///< "pump_input" / "xp_pump_input": into the backpack
    PumpOutput,  ///< "pump_output": out of the backpack
    World,       ///< "interact_with_world"
    Hand,        ///< "interact_with_hand"
    XpOutput,    ///< "xp_pump_output"
    XpKeep,      ///< "xp_pump_keep"
    Off,         ///< "xp_pump_off"
    Mend,        ///< "mend_items"
    TakeAll,     ///< "take_all_experience"
    Take,
    Store,
    StoreAll,    ///< "store_all_experience"
    TankInput,   ///< empty tank input slot
    TankOutput,  ///< empty tank output slot
    BatteryInput,
    BatteryOutput,
};
/// A 16x16 icon at (x, y).
void icon(Painter& p, Icon i, float x, float y, float alpha = 1);
/// The "do not ..." variants: a red no-entry sign over a 16x16 icon at (x, y).
void noSign(Painter& p, float x, float y);

/// Java "%,d".
[[nodiscard]] std::string grouped(long long n);

/// A slot inside an upgrade tab showing item `index` of the tab's server menu (`tab["menu"]`);
/// clicks go to that menu slot (`menuClick`). Empty: `emptyIcon` faded plus `emptyTooltip`.
class MenuSlot : public Widget {
public:
    int                                                   index = 0;
    Icon                                                  emptyIcon = Icon::TankInput;
    bool                                                  drawEmptyIcon = true;
    std::string                                           emptyTooltip;
    std::function<void(nlohmann::json)>                   send;
    std::vector<ItemStack>*                               stacks = nullptr; ///< owned by the tab
    std::vector<long long>*                               counts = nullptr;

    void render(Painter& p, float mx, float my, double time) override;
    bool mouseDown(MouseButton button, float x, float y) override;
    void tooltip(float x, float y, std::vector<TooltipLine>& out) const override;
};

/// Reads `tab["menu"]["slots"]` into stacks / counts.
void readMenuSlots(nlohmann::json const& tab, std::vector<ItemStack>& stacks, std::vector<long long>& counts);

} // namespace abp::client::ui::fluidgfx
