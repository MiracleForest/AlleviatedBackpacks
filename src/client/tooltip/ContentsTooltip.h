#pragma once

// Item tooltips (client):
//
// - Backpack items (`BackpackItem.appendHoverText`, `BackpackItemClient.getTooltipImage`):
//     "Press <Left Shift> to View Contents" (gray, "Left Shift" aqua) while Shift is up;
//     with advanced tooltips "UUID: <uuid>" (dark gray);
//     with Shift held (or an item on the cursor) the contents image
//     (`ClientStorageContentsTooltipBase`): the stack multiplier / tank / energy lines, "Upgrades"
//     with on/off pips (4x10) for upgrades that can be disabled, "Inventory" with the contents
//     compacted and sorted by count, 9 per row, cells max(18, width(count) + 2) wide.
//     The data comes from the server (ContentsPacket), asked for at most every 20 ticks.
// - Upgrade items (`UpgradeItemBase.appendHoverText`): the `<descriptionId>.tooltip` lines
//   from the resource pack's lang, dark gray.

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "client/ui/widgets/Tooltip.h"

class ItemStack;

namespace abp::client::tooltip {

/// Shift held or something on the cursor (BackpackItemClient condition for the contents image).
[[nodiscard]] bool contentsWanted();
/// Java's advanced tooltips (F3+H, toggled by input/InputRouter): backpacks show "UUID: ...".
/// Off at start. Any thread.
[[nodiscard]] bool advancedTooltips();
void               setAdvancedTooltips(bool on);
/// "§7Press <§bLeft Shift§7> to View Contents" in the game's language.
[[nodiscard]] std::string pressForContentsLine();
/// The tooltip lines of a backpack item (vanilla lines, with the "press" line when the contents
/// are not shown) and, when shown, draws the contents image below. Returns false if `stack` is not
/// a backpack.
bool drawBackpackTooltip(ui::Painter& painter, ItemStack const& stack, float x, float y, std::vector<ui::TooltipLine> extra = {});
/// `ClientEventHandler.renderSpecialTooltip`: "Right Click To Add" and the contents image of
/// `backpack` (right-click stashing).
void drawRightClickToAdd(ui::Painter& painter, ItemStack const& backpack, float x, float y);

} // namespace abp::client::tooltip
