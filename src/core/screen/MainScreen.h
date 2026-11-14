#pragma once

// Pure rules of the storage screen (`StorageScreenBase`) that the native backpack
// screen needs: slot count overlays, the quick craft (drag) preview, the "Count" tooltip line and
// the error message wrapping. No game types: unit-tested in tests/MainScreenTest.cpp.

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace abp::core::screen {

/// What the count overlay of a slot shows (`renderStack` / `renderStackCount`).
struct CountOverlay {
    std::string text;            ///< empty = nothing (count <= 1 and no override)
    bool        special = false; ///< count > 99: abbreviated and scaled to 16 units (`renderStackCount`)
    bool        yellow  = false; ///< capped drag preview (`ChatFormatting.YELLOW`)
};

/// Overlay of a stack with `count` items. `overrideText` (e.g. the capped drag preview) replaces the
/// number (`renderSlot` passes it to `renderStackCount`).
[[nodiscard]] CountOverlay countOverlay(long long count, std::string overrideText = {}, bool yellow = false);

/// `renderStackCount` scale: `min(1, 16 / width)`.
[[nodiscard]] float countScale(float textWidth);

/// Vanilla quick craft types (`quickCraftingType`): 0 = split evenly (left), 1 = one each (right),
/// 2 = full stacks (middle, creative).
enum class QuickCraft { Split = 0, One = 1, Clone = 2 };

/// `StorageContainerMenuBase.getQuickCraftPlaceCount`: how many of the carried stack one of `slots` dragged-over slots
/// gets (capped by `slotMax` = `slot.getMaxStackSize(carried)`).
[[nodiscard]] int quickCraftPlaceCount(QuickCraft type, int carriedCount, int carriedMaxStack, int slots, int slotMax);

/// The preview of one dragged-over slot (`renderSlot` while quick crafting).
struct DragPreview {
    int          count = 0;  ///< count to draw (`place + slotCount`)
    CountOverlay overlay;    ///< count text (yellow capped limit when it does not fit)
};
/// `slotCount` = items already in the slot (0 = empty), `slotLimit` = `slot.getMaxStackSize(stack in
/// the slot)` (`renderSlot` uses 64 for an empty slot).
[[nodiscard]] DragPreview dragPreview(int place, int slotCount, bool slotEmpty, int slotLimit);

/// `recalculateQuickCraftRemaining`: what stays on the cursor. `slots` holds per dragged-over
/// slot {items already in it, its max for the carried item}.
struct DragSlot {
    int count = 0;
    int max   = 64;
};
[[nodiscard]] int quickCraftRemainder(QuickCraft type, int carriedCount, int carriedMaxStack, std::vector<DragSlot> const& slots);

/// Whether the hovered storage slot's tooltip gets the "Count: n / max" line (
/// `getTooltipFromContainerItem`): the slot's own
/// limit (`baseSlotLimit`, 64 x multiplier) differs from the item's max stack size.
[[nodiscard]] bool showsCountLine(int baseSlotLimit, int itemMaxStack);

/// Splits `text` into lines no wider than `maxWidth` (vanilla `Font.split`: breaks at spaces, a
/// word longer than the line is cut). `width` measures a string.
[[nodiscard]] std::vector<std::string>
wrapText(std::string_view text, float maxWidth, std::function<float(std::string_view)> const& width);

/// `renderErrorMessage` geometry for the wrapped lines: height of the text block
/// (8, plus 2 + 10 per extra line).
[[nodiscard]] int errorTextHeight(std::size_t lines);

} // namespace abp::core::screen
