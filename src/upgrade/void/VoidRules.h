#pragma once

// Pure decision rules of the Void upgrade (no game headers; unit-tested in tests/), following the
// behaviour of `VoidUpgradeWrapper` together with the overflow handling of
// `InventoryHandler` / `InventoryHandlerSlotTracker` / `StorageContainerMenuBase`.
//
// Two modes (`shouldVoidOverflow`): void every matching item ("Void Any"), or only void
// what overflows ("Void Overflow").

#include <functional>

#include "core/BackpackInventory.h"
#include "core/Filter.h"

namespace abp::upgrade::voiding {

/// What one installed Void upgrade decides with.
struct VoidRules {
    bool          overflowOnly    = false; ///< effective `shouldVoidOverflow()`
    bool          workInGui       = false;
    core::MatchBy matchBy         = core::MatchBy::Item;
    bool          matchDurability = false;
    bool          matchNbt        = false;
    /// `filterLogic.matchesFilter(stack)`.
    std::function<bool(core::ItemStackData const&)> matches;
};

/// `IOverflowResponseUpgrade.stackMatchesFilterStack(stored, incoming)`: the same item;
/// with tag matching nothing else counts, otherwise durability / NBT when the filter matches them.
[[nodiscard]] inline bool overflowStacksMatch(VoidRules const& r, core::ItemStackData const& stored, core::ItemStackData const& incoming) {
    if (stored.id != incoming.id || stored.aux != incoming.aux) return false;
    if (r.matchBy == core::MatchBy::Tag) return true;
    if (r.matchDurability && stored.damage != incoming.damage) return false;
    return !r.matchNbt || (stored.nbtHash() == incoming.nbtHash() && stored.nbt() == incoming.nbt());
}

/// `onBeforeInsert` skips its "another slot already has it" check when the filter matches
/// item + NBT + durability exactly.
[[nodiscard]] inline bool exactMatching(VoidRules const& r) {
    return r.matchNbt && r.matchDurability && r.matchBy == core::MatchBy::Item;
}

/// Same item, NBT and damage (`ItemStackKey`).
[[nodiscard]] inline bool sameKey(core::ItemStackData const& a, core::ItemStackData const& b) {
    return a.sameItem(b) && a.damage == b.damage;
}

[[nodiscard]] inline bool isFull(core::BackpackInventory const& inv, int slot) {
    auto const& s = inv.at(slot);
    return !s.empty() && s.count >= inv.slotLimit(slot, s);
}

/// A full stack of exactly this item (the slot tracker `fullStackSlots`).
[[nodiscard]] inline bool hasFullStack(core::BackpackInventory const& inv, core::ItemStackData const& item) {
    for (int i = 0; i < inv.size(); ++i) {
        if (sameKey(inv.at(i), item) && isFull(inv, i)) return true;
    }
    return false;
}

/// Any stack (full or not) of exactly this item.
[[nodiscard]] inline bool hasStack(core::BackpackInventory const& inv, core::ItemStackData const& item) {
    for (int i = 0; i < inv.size(); ++i) {
        if (sameKey(inv.at(i), item)) return true;
    }
    return false;
}

/// A stack that is not full yet of exactly this item (the slot tracker `partiallyFilledStackSlots`).
[[nodiscard]] inline bool hasPartialStack(core::BackpackInventory const& inv, core::ItemStackData const& item) {
    for (int i = 0; i < inv.size(); ++i) {
        if (sameKey(inv.at(i), item) && !isFull(inv, i)) return true;
    }
    return false;
}

/// Some other slot holds a stack that `overflowStacksMatch` the incoming item.
[[nodiscard]] inline bool hasOverflowMatch(VoidRules const& r, core::BackpackInventory const& inv, core::ItemStackData const& item, int ignoreSlot = -1) {
    for (int i = 0; i < inv.size(); ++i) {
        if (i == ignoreSlot) continue;
        auto const& s = inv.at(i);
        if (!s.empty() && overflowStacksMatch(r, s, item)) return true;
    }
    return false;
}

[[nodiscard]] inline bool hasEmptySlot(core::BackpackInventory const& inv) {
    for (int i = 0; i < inv.size(); ++i) {
        if (inv.at(i).empty()) return true;
    }
    return false;
}

/// The insert verdict of one Void upgrade.
///
/// Automatic inserts (pickup, automation, other upgrades) follow the insert path. Nothing is
/// voided when the storage has neither an empty slot nor a stack of the item (the mod rejects the stack
/// before any upgrade sees it). Then, for matching items:
/// - "Void Any": the item is voided;
/// - "Void Overflow": a full stack of the item already stored voids it all; otherwise stacks of the
///   item are topped up and the rest is voided; a new stack is only started when no other slot
///   holds the item (with exact item + NBT + durability matching, a new stack may be started next
///   to a partial one).
///
/// Player inserts (GUI) only void with "work in GUI" (either mode, as the menu overflow handling):
/// placing the item into an empty slot while another slot holds it voids it, and what does not fit
/// the clicked stack is voided. "Void Any" also empties matching slots on the next tick (see the
/// upgrade).
[[nodiscard]] inline core::InsertVerdict verdict(VoidRules const& r, core::InsertQuery const& q) {
    using V = core::InsertVerdict;
    auto const& inv = q.inventory;
    if (q.ctx.source == core::Source::Internal) return V::Allow;
    // The structural checks come first: `insert` asks once per slot, the filter is the costly part.
    auto matches = [&] { return r.matches && r.matches(q.stack); };

    if (q.ctx.source == core::Source::Player) {
        if (!r.workInGui) return V::Allow;
        if (q.slot < 0) return matches() && hasOverflowMatch(r, inv, q.stack) ? V::VoidOverflow : V::Allow;
        auto const& cur = inv.at(q.slot);
        if (cur.empty()) return matches() && hasOverflowMatch(r, inv, q.stack, q.slot) ? V::Void : V::Allow;
        return q.direct && cur.sameItem(q.stack) && matches() ? V::VoidOverflow : V::Allow;
    }

    if (!r.overflowOnly) {
        if (q.slot >= 0) return q.direct && matches() ? V::Void : V::Allow;
        return matches() && (hasEmptySlot(inv) || hasStack(inv, q.stack)) ? V::Void : V::Allow;
    }

    // "Void Overflow".
    if (q.slot < 0) {
        if (!matches()) return V::Allow;
        if (hasFullStack(inv, q.stack)) return V::Void;
        // Whatever is left once the stacks of the item are topped up (and, if needed, one new stack
        // is started) is voided; the per-slot checks below keep further stacks from being started.
        return hasPartialStack(inv, q.stack) || hasEmptySlot(inv) ? V::VoidOverflow : V::Allow;
    }
    auto const& cur = inv.at(q.slot);
    if (!q.direct) {
        // Automatic placement into an empty slot (`onBeforeInsert`): skipped when another
        // slot already holds the item.
        return cur.empty() && !exactMatching(r) && matches() && hasOverflowMatch(r, inv, q.stack, q.slot) ? V::Void : V::Allow;
    }
    // A caller-chosen slot (automation): the mod ignores the slot and runs the same path as above.
    if (!matches()) return V::Allow;
    if (hasFullStack(inv, q.stack)) return V::Void;
    if (cur.empty()) return !exactMatching(r) && hasOverflowMatch(r, inv, q.stack, q.slot) ? V::Void : V::Allow;
    return sameKey(cur, q.stack) ? V::VoidOverflow : V::Allow;
}

} // namespace abp::upgrade::voiding
