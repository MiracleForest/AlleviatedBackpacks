#pragma once

// Duplicate storage uuid handling
// (`UUIDDeduplicator` and the dedupe pass of `CommonEventHandler.onWorldTick`): backpack items that
// share a storage uuid (duplication glitches, mods that copy drops, `/backpackadmin give` to several players)
// are sorted out so only one item keeps the storage. The others lose the uuid and get a fresh,
// empty storage when opened.
//
// - When a backpack item entity appears: if another live backpack item entity within 10 blocks
//   carries the same uuid, the new one loses its uuid.
// - Every 40 ticks per dimension (unless `tickDedupeLogicDisabled`): the backpacks of every player
//   in that dimension, in scan order; the first one seen keeps the uuid, later copies lose it.
// - When a player opens a backpack: the player's other backpacks with that uuid lose it.

class ItemActor;
class ItemStack;
class Player;
class BlockSource;

#include "game/PlayerScan.h"

namespace abp::dedup {

/// Removes the storage uuid from a backpack item (removeContentsUUIDTag).
void removeUuid(ItemStack& stack);

/// Checks a new backpack item entity against the item entities within 10 blocks; the new entity
/// loses its uuid on a match. Returns how many uuids were removed (0 or 1).
int dedupeItemEntityInArea(ItemActor& entity, BlockSource& region);
/// Dedupes the backpacks of the players in `dimension` (the periodic check). Returns how many
/// uuids were removed.
int dedupePlayers(int dimension);
/// Removes the uuid of `player`'s other backpacks that share it with the one at `openedAt`
/// (opening a backpack). Returns how many uuids were removed.
int dedupeOnOpen(Player& player, game::ItemLocation openedAt);

} // namespace abp::dedup
