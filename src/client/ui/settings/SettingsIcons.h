#pragma once

// Icons of the settings screen that the shared AbpStyle set doesn't have (original pixel art, drawn
// from the icon sizes only; no texture is copied).

#include <cstdint>

#include "client/ui/gfx/Painter.h"

namespace abp::client::ui::settings {

enum class Icon {
    ItemDisplayTab,   ///< 16x16 tab icon
    Rotate,           ///< 16x16
    SaveTemplate,     ///< 16x16
    LoadTemplate,     ///< 16x16
    ExportTemplate,   ///< 16x16
    AnotherPlayerOn,  ///< 16x16
    AnotherPlayerOff, ///< 16x16
};

void icon(Painter& p, Icon which, float x, float y, float alpha = 1);

/// ItemDisplaySettingsTab.SLOT_SELECTION (24x24): the frame around the item display's
/// selected slot, drawn at (slot x - 4, slot y - 4).
void slotSelection(Painter& p, float x, float y);

/// The memorized stack overlay (StorageScreenBase.drawStackOverlay, 16x16): the faded veil
/// over a memory slot.
void memoryOverlay(Painter& p, float x, float y);

} // namespace abp::client::ui::settings
