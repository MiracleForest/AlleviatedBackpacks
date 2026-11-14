#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/BackpackInventory.h"
#include "core/Filter.h"

namespace abp::core {

/// How the Sort button orders items.
enum class SortBy : int {
    Name  = 0, ///< by item name (id path, then namespace)
    Count = 1, ///< by total count, most first
    Mod   = 2, ///< by namespace ("mod"), then name
    Tag   = 3, ///< by the item's first tag (items without tags last), then name
};
inline constexpr int kSortByCount = 4;

[[nodiscard]] std::string_view       toString(SortBy by);
[[nodiscard]] std::optional<SortBy>  sortByFromString(std::string_view s);
/// The next mode in `SortBy` order Name -> Mod -> Count -> Tag (wraps around), for the
/// sort-by button.
[[nodiscard]] SortBy                 nextSortBy(SortBy by);

using ItemLess = std::function<bool(ItemStackData const&, ItemStackData const&)>;
/// Display name of an item (what players see), for `SortBy::Name` / `SortBy::Mod`.
using ItemNameFn = std::function<std::string(ItemStackData const&)>;

/// A strict weak ordering of item kinds for `by`, like the `InventorySorter` comparators:
/// - Name: display name (lower case);
/// - Mod: namespace, then display name;
/// - Count: total count (most first), then item id;
/// - Tag: number of tags (most first), then the sorted tag names, then item id.
/// The items passed in are merged totals (one per kind), so `Count` compares totals. `tags` is
/// needed for `SortBy::Tag` (nullptr: no tags); `names` gives display names (nullptr: id path).
[[nodiscard]] ItemLess comparatorFor(SortBy by, ITagProvider const* tags = nullptr, ItemNameFn names = nullptr);

/// Per-slot sorting constraints.
struct SortLayout {
    /// No-sort slots. Shorter than the inventory = rest false.
    std::vector<bool>          locked;
    /// Memory slots: a non-empty entry is the remembered item of that slot. Sorting treats them
    /// like no-sort slots (`BackpackWrapper.sort` excludes both).
    std::vector<ItemStackData> memory;
    /// MemorySettingsCategory with `ignoreNbt` off ("Match NBT"): a memory slot only takes
    /// the exact remembered stack (id, aux and NBT; `ItemStackKey.matches`), not every variant.
    bool                       memoryMatchNbt = false;
};

/// True if `item` may go into a memory slot remembering `remembered` (same id and aux; NBT is
/// ignored so damaged / enchanted variants still match).
[[nodiscard]] bool memoryMatches(ItemStackData const& remembered, ItemStackData const& item);
/// `MemorySettingsCategory.matchesFilter` in the layout's mode: `memoryMatches` when NBT is
/// ignored, the exact item (`sameItem`) when it is matched.
[[nodiscard]] bool memorySlotAccepts(SortLayout const& layout, ItemStackData const& remembered, ItemStackData const& item);

/// Sorts `inv` like `InventorySorter.sortHandler`: no-sort and memory slots are excluded
/// (their items are not moved out); the other slots' items are merged per kind and ordered with
/// `less`; an excluded slot already holding a kind is first topped up with it (up to its limit),
/// and the rest fills the other slots in order. Uses `inv.slotLimit`.
/// Returns false (and changes nothing) if the result would not fit.
bool sortInventory(BackpackInventory& inv, ItemLess const& less, SortLayout const& layout = {});

} // namespace abp::core
