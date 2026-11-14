# Features

This file lists every feature of AlleviatedBackpacks for Minecraft Bedrock with its status and the
source files that implement it. Class names in the rows (e.g. `BackpackWrapper`) name the behaviour
each row describes. The map is at roughly class or feature granularity and covers upgrades,
backpacks, world interaction, client GUI, settings, crafting, commands, data, networking,
configuration and lang files (`src/**` and `assets/**`).

Statuses:

- **Done**: implemented with the documented behaviour, numbers, defaults, texts and controls.
  Bedrock-only mechanics are noted where they differ only in form (e.g. LevelDB instead of NBT files).
- **PLATFORM-LIMIT**: Bedrock (or LeviLamina) has no way to do it; the row says why.
- **Extra**: an AlleviatedBackpacks addition.
- **N/A**: compat with Java-only mods (Chipped, Sawmill; also Curios, Accessories, Botania,
  JEI/EMI/REI, Create, FTB Chunks, Mouse Tweaks, Inventory Sorter, ...) and Java-only internals.

"GX-1" .. "GX-5" in a row name the unit that closed the gap (GX-1 rendering, GX-2 upgrade / world
behaviour, GX-3 GUI / settings / input, GX-4 the feature audit, GX-5 the audit's leftovers). The
file was re-verified against the code of `main` by the final integration run.

## Items, tiers and recipes

| Feature | Status | Where | Notes |
|---|---|---|---|
| Six backpacks: leather (27/1), copper (45/1), iron (54/2), gold (81/3), diamond (108/5), netherite (120/7) | Done | `core/Tier.h`, `feature/admin/AbpConfig.*` | Slot counts come from `alleviatedbackpacks-server.json` (`<tier>Backpack.inventorySlotCount/upgradeSlotCount`, 1..144 / 0..10) |
| Backpacks never stack; every upgrade and downgrade item stacks to 64; upgrade base 16 | Done (GX-4) | `assets/pack/bp/items/*.json` | Before: 1 or 16 for most upgrades |
| Netherite backpack is fire resistant as an item | Done (GX-4) | `netherite_backpack.json` (`minecraft:fire_resistant`) | |
| Backpacks can't go into bundles or shulker boxes (`canBePlacedInContainerItems` = false, the default) | Done | `feature/bundle/BundleRule.cpp`, `feature/admin/AbpConfig.cpp` | Bundles (server validation and companion client prediction) and shulker boxes (screen and hoppers) follow the config |
| `canBePlacedInContainerItems = true` lets backpacks into bundles | Done | `feature/bundle/BundleRule.cpp`, `client/bundle/BundleRuleClient.cpp` | The server's bundle validation (`DynamicContainerValidation::isItemAllowedInSlot`) follows the live config (`/backpack reload` applies); companion clients get the rule (`BundleRulePacket`) and refuse the move before sending it. A backpack weighs a whole bundle (Java 1/maxStackSize, Bedrock 64 of 64). The items' `storage_weight_modifier` never loaded (format 1.21.40) and was removed |
| Backpack recipe, tier upgrade recipes keeping contents, netherite smithing | Done | `assets/pack/bp/recipes/`, `feature/tiers/TierCrafting.cpp`, `feature/tiers/SmithingBase.cpp` | GX-3: the Diamond backpack has no durability; Bedrock's recipe-load check "base item is not a piece of armor or equipment" is answered by a hook (`Recipes::_loadRecipe` + `ComponentItem::isDamageable`). Selftest `tiers.smithing_without_durability`, client e2e `gx3` (smithing table) |
| `backpack_dye` (1-8 dyes, main/accent by column) | Done | `recipes/dye/`, `feature/tiers/DyeTag.cpp` | |
| Cauldron wash resets main and accent colour | Done (GX-4) | `feature/tiers/TierLore.cpp` | Before: only the main colour |
| All regular upgrade recipes (patterns, keys, counts) | Done | `assets/pack/bp/recipes/*.json` | |
| Ingredient tags `c:chests/wooden`, `c:chests`, `c:glass_blocks`, `c:obsidians` | Done (GX-4) | `feature/tiers/DyeTag.cpp` (item tags `alleviatedbackpacks:wooden_chests/chests/glass_blocks/obsidians`) | Before: chest, glass and obsidian only |
| `upgrade_next_tier` keeps the upgrade's data | Done | `TierCrafting.cpp` (`carryUpgradeData`) | |
| `upgrade_clear` (an upgrade alone in the grid loses its data) | Done (GX-4) | `assets/pack/bp/recipes/clear/*.json` | |
| Hopper / Advanced Hopper upgrade | Extra | `upgrade/hopper` | Hopper upgrades for backpacks |
| `xp_bucket` | PLATFORM-LIMIT | | Bedrock has no custom fluids or fluid buckets; experience is stored as a tank fluid (`abp:experience`) |
| Chest loot (9 tables, weights, `chestLootEnabled`) | Done | `feature/wearable/ChestLoot.cpp` | |

## Backpack item, block and world

| Feature (class) | Status | Where | Notes |
|---|---|---|---|
| `BackpackItem.use` / sneak to place (facing, waterlogged, wool sounds, creative keeps item) | Done | `feature/placed/PlacedBackpacks.cpp` | |
| `BackpackItem.cloneBackpack` -> `cloneSubbackpacks` (creative placement) | Done | `PlacedBackpacks::placeFromHand` | The placed clone and every backpack directly inside it get new storages (one level). Selftest `placed.creative_clone` |
| `inventoryTick` skips spectators and the dead | Done (GX-4) | `game/PlayerTicker.cpp` | |
| `onDroppedByPlayer` (the open backpack can't be dropped) | Done | `net/NativeScreens.cpp` | |
| `makesPiglinsNeutral` (gold backpack worn) | Done (GX-5) | `feature/wearable/PiglinNeutral.cpp` | A worn Gold Backpack counts as gold armour: the piglin's "player without golden armour" target entry is skipped for its wearer (`TargetGoal::_matchesTargetTypes`); its other entries (open container, block breaking, hurt-by) still anger it |
| `createEntity` / UUID deduplication | Done | `feature/dedup` | |
| Everlasting item entity (no despawn, floats from void/lava, fire immune, particles) | Done | `upgrade/everlasting`, `feature/placed/EntityGriefing.cpp` | Withers and the dragon can't destroy it (GX-2, selftest `everlasting.wither_and_dragon`). Explosions no longer push it (`ignoreExplosion`, GX-5: Bedrock pushed it 1.6 blocks; `Explosion::explode` now restores its motion) |
| `BackpackBlock`: open, sneak pick-up, comparator, waterlogging, states, hardness, pistons | Done | `feature/placed` | |
| `BackpackBlock.useItemOn` with a fluid container: fill/drain tanks, bucket sounds; a bucket does nothing without tanks | Done (GX-4) | `PlacedBackpacks::useFluidContainer` | Before: no sound; a bucket on a backpack without tanks opened the GUI |
| Dispenser places a backpack (`BackpackDispenseBehavior`) | Done (GX-5) | `feature/placed/Dispense.cpp` | In front of the dispenser, facing it (north for up / down); nothing happens (the backpack stays) when it can't be placed |
| `BackpackBlockEntity`: placed ticks, hopper/automation I/O, `noConnectionBlocks`, loot on first access | Done | `feature/placed`, `feature/admin/Connections.cpp` | |
| `BackpackAccessLogger`, `/backpackadmin list/give/removeNonPlayer` | Done | `feature/admin` | Same texts and arguments; names and uuids autocomplete from the access log. Storages are only removed by `removeNonPlayer`, never automatically on load |
| Clickable / hoverable `/backpackadmin list` lines | PLATFORM-LIMIT | `feature/admin/AbpAdminCommands.cpp` | Bedrock chat has no click or hover events |
| Opening another player's worn backpack | Done | `feature/access` | |
| Mob backpacks (`EntityBackpackAdditionHandler`: spawn chance, fixed tier weights by local difficulty, loot, potions, armour, jukebox, drops) | Done | `feature/wearable/MobBackpacks.cpp` | Mob jukebox disc count follows `advancedJukeboxUpgrade` size (GX-4). Creepers lose their buffs (`removeBeneficialEffects`). The mod has no handling for mobs that convert (zombie to drowned, ...) and no config for tier weights or minimum tiers |
| Too-many-backpacks slowness, `onlyWornBackpackTriggersUpgrades` | Done | `feature/nerf` | |
| Automatic insert order: same-item stacks, then empty memory slots, then empty slots | Done (GX-4) | `core/BackpackInventory.cpp` | Before: memory slots first |

## Upgrades

| Upgrade (classes) | Status | Where | Notes |
|---|---|---|---|
| Pickup / Advanced Pickup | Done | `upgrade/pickup` | |
| Pickup upgrade awards the "item picked up" statistic | PLATFORM-LIMIT | `upgrade/UpgradeManager.cpp` | Bedrock has no player statistics (`Stats.ITEM_PICKED_UP`) |
| Filter / Advanced Filter (direction, contents filter) | Done | `upgrade/filter` | |
| Several Filter upgrades combine as `FilteredItemHandler`: an item passes when any one filter passes | Done (GX-4) | `FilterUpgrade.cpp` | Before: always AND |
| Filter logic (item/mod/tags, durability, NBT, any/all tags, primary match) | Done | `core/Filter.*`, `upgrade/filter/FilterCommon.cpp` | A tag without a namespace is `minecraft:` (ResourceLocation), so `logs` and `minecraft:logs` are one tag (GX-5) |
| Tag list order (TreeSet<ResourceLocation>: path, then namespace) | Done (GX-4) | `core::tagLess` | Before: plain string order |
| Magnet / Advanced Magnet (range 3/5, cooldowns, XP into tanks) | Done | `upgrade/magnet` | |
| Feeding / Advanced Feeding | Done | `upgrade/feeding` | |
| Compacting / Advanced Compacting | Done | `upgrade/compacting` | Recipe leftovers are kept (GX-2, selftest `compacting.recipe_remainders`) |
| Void / Advanced Void | Done | `upgrade/void` | |
| Restock / Deposit (+ advanced) | Done | `upgrade/restock`, `upgrade/deposit` | With Inception they use the combined inventories (GX-2) |
| Refill / Advanced Refill (6/12 slots, targets, pick block) | Done | `upgrade/refill` | Slot counts configurable in `config/refill.json` (GX-4) |
| Stack upgrades (starter..omega, downgrades) | Done | `upgrade/stack` | |
| Smelting / Smoking / Blasting and Auto versions | Done | `upgrade/cooking` | Flame / smoke particles and the furnace crackle (10% per render call) on worn and placed backpacks; placed ones at Java's animateTick rate (GX-1) |
| Crafting | Done | `upgrade/crafting` | Several per backpack, each with its own grid, results and open-tab state (GX-5) |
| Stonecutter (`upgrades/stonecutter`) | Done | `upgrade/stonecutter` | |
| Jukebox / Advanced Jukebox | Done | `upgrade/jukebox` | |
| Tank (capacity, I/O rates, container slots, cooldown) | Done | `upgrade/tank` | Tank bars use the game's animated still textures (water_still_grey tinted 0x3F76E4, lava_still, milk, potions tinted by potion colour; frame-blended flipbooks) (GX-1). The red error overlay on tank / battery bars when a Stack Upgrade removal / swap / downgrade check fails (GX-1). Model tanks: the real animated sprites (water_still, lava_still, grey water for milk / potions) |
| Per-potion tint of the fluid in the 3D model's tanks | PLATFORM-LIMIT | `client/render` | Attachables get their shader constants once per attachable (`ActorShaderManager::setEntityConstants`; render-controller `color` / `overlay_color` / `uv_anim` never reach it) and those constants already carry the cloth / border colours; potions show untinted grey water on the model (the GUI tank bars are tinted) |
| Battery (FE storage, I/O) | Done | `upgrade/battery` | Energy API: `docs/API.md` |
| Pump / Advanced Pump | Done | `upgrade/pump` | |
| XP Pump (levels, mending) | Done | `upgrade/xppump` | |
| Infinity / Survival Infinity | Done | `upgrade/infinity` | Item tooltips added (GX-4) |
| Everlasting, Inception, Tool Swapper "already in another upgrade slot" messages | Done (GX-4) | `UpgradeInfo::existsKey` | Before: the generic "Only 1x ..." text |
| Inception (nesting, both config flags) | Done | `upgrade/inception` | Combined inventories for every upgrade that uses them (GX-2, selftest `inception.combined_inventory_users`) |
| Tool Swapper / Advanced Tool Swapper (click/attack swaps, key, modes) | Done | `upgrade/toolswapper` | The swap key also works in creative (GX-4) |
| Advanced Tool Swapper filter with TAGS mode (`FilterLogicControl.Advanced`) | Done (GX-4) | `client/ui/tabs/toolswapper`, `toolswapper/Upgrades.cpp` | Slot count from `config/toolswapper.json` `toolSwapperUpgrade` |
| Anvil, Smithing | Done | `upgrade/anvil`, `upgrade/smithing` | |
| Upgrade limits per backpack: the mod only limits through `maxUpgradesPerStorage` (stack 3, furnace 1, jukebox 1) and its conflict rules | Done (GX-5) | `admin::maxUpgradesHit`, `NativeScreens.cpp` `installError` | Upgrades the mod doesn't limit have `kUnlimitedPerBackpack`; the three groups follow the config (item entries before group entries, `\|0` = not allowed), and a refusal highlights the upgrades of that item / group (`checkUpgradePerStorageTypeLimit`). `StackConfig::maxPerBackpack` is gone |
| Chipped / Sawmill compat upgrades | N/A | | Chipped and Sawmill are Java-only mods |

## Configuration

| Key | Status | Notes |
|---|---|---|
| `alleviatedbackpacks-server.json`: disallowed items, container items, no-interaction/no-connection blocks, slot counts, entity additions, nerfs, `maxUpgradesPerStorage`, fluid handler, other-player opening, item display, dedup, `canBePlacedInContainerItems` | Done | `allowOpeningOtherPlayerBackpacks` and `itemDisplayDisabled` were split with `gui.json`: both files now count (off in either = off) (GX-4) |
| `alleviatedbackpacks-common.json` `chestLootEnabled`, `alleviatedbackpacks-core.json` `enabledItems` | Done | |
| Per-upgrade sections (filter slots, ranges, multipliers, tank/battery/pump/xp pump, cooking, compacting, inception) | Done | Refill and Tool Swapper sections added (GX-4) |
| Jukebox `advancedJukeboxUpgrade.numberOfSlots/slotsInRow` | Done (GX-5) | Sectioned keys (`config/jukebox.json` v3); the old flat keys migrate |
| Client `sortButtonsPosition`, `playButtonSound` | Done | `client/ui/screen/ScreenConfig.*` |
| Unopened backpack lore uses the configured slot counts | Done (GX-4) | `TierLore.cpp` |

## Client GUI, input and rendering

| Feature (class) | Status | Where | Notes |
|---|---|---|---|
| StorageScreenBase layout, upgrade column, switches, sort/transfer buttons, search box, scroll panel, counts, memory/no-sort overlays, error overlay | Done | `client/ui/screen/BackpackScreen.cpp`, `core/screen` | GX-3: the carried item is drawn below the sort buttons, upgrade switches and the error box (`StorageScreenBase` render order) |
| Closed tabs drawn unless fully covered; tooltip only when the open tab doesn't cover its top | Done (GX-4) | `BackpackScreen.cpp` | The two checks were swapped |
| Storage settings tab opens on any mouse button | Done (GX-4) | `BackpackScreen.cpp` | |
| XP Pump action buttons work with any mouse button | Done (GX-4) | `tabs/xppump/XpPumpTab.cpp` | |
| Empty upgrade slot icon | Done (GX-3) | `AbpStyle.cpp`, `BackpackScreen.cpp` | Original outline art, opaque, in the slot's 16x16 item area (the upgrade slot background) |
| Switching an upgrade on/off closes the open tab (`StorageUpgradeSlot.setChanged` -> `reloadUpgradeControl`) | Done (GX-3) | `BackpackScreen::rebuildTabs`, `NativeScreens.cpp` | The enabled flags are part of the tab signature; the server drops the open tab id |
| Enter in text boxes keeps focus | Done (GX-3) | `widgets/TextBox.cpp` | |
| Truncated open-tab title tooltip | Done (GX-3) | `BackpackScreen.cpp` | |
| Clicking a tab border doesn't throw the cursor item | Done (GX-3) | `BackpackScreen::onMouseButton` | `StorageScreenBase.hasClickedOutside`: upgrade slots rectangle and tab rectangles |
| Middle-click sort anywhere but the player inventory, before other handlers | Done (GX-3) | `BackpackScreen::onMouseButton` | |
| Drag distribution over upgrade slots | Done (GX-3) | `BackpackScreen::canDragTo`, `NativeScreens.cpp` | One upgrade per empty slot |
| Q / 1-9 over upgrade slots; bound (not literal) keys | Done (GX-3) | `BackpackScreen::onKey`, `NativeScreens.cpp`, `input/Hotkeys.cpp` | Inventory / drop / hotbar keys are read from the keyboard layout (`key.inventory`, `key.drop`, `key.hotbar.N`); drop and hotbar swap work on upgrade slots with the remove / install checks |
| Jukebox playtime overlay updates smoothly | Done (GX-3) | `tabs/jukebox` | The client advances the remaining time every frame between the server's whole-second updates (`JukeboxUpgradeTab` computes it from the game time) |
| Crafting "Previous" tooltip on the left | Done (GX-3) | `tabs/crafting`, `widgets/Tooltip.*` | `CraftingUpgradeTab` LEFT_SIDE_TOOLTIP_POSITIONER |
| Stonecutter recipe list, anvil name box, smithing help tooltips | Done (GX-3) | `tabs/stonecutter`, `upgrade/anvil/AnvilTab.cpp`, `upgrade/smithing` | Stonecutter: `StonecutterRecipeControl` (4x3 list with scroll track, no expanded list or search); anvil box padding 1 / 86 wide, right click doesn't clear; onboarding tooltips with an item carried; recipe tooltip hit box from x (clicks from x + 1) |
| Settings screens (main, memory, no sort, item display, templates) | Done | `client/ui/settings`, `feature/gui` | Item Display preview draws the real 3D model (display entity with the backpack attachable and its display item) with the rotation and drag (GX-1). GX-3: the open tab id is kept on the storage (server side, removed when Keep Tab Open is off, like `removeOpenTabIfKeepOff`); Match-NBT memory slots take only the exact stack when sorting, in filter-by-storage and in the stash overlay |
| Key bindings (names, defaults, category, open/inventory interaction/tool swap/toggles, sort) | Done | `client/input` | No transfer keys and no separate key category |
| Right-click stashing | Done | `client/input/Stash.cpp`, `feature/stash` | GX-3: any vanilla container slot (crafting grids, furnace / blast furnace / smoker incl. result with XP, brewing stand, stations, crafter, horse); main inventory slot indices fixed |
| Backpack tooltip and Shift contents tooltip | Done | `client/tooltip` | GX-3: long upgrade tooltip lines wrap by NeoForge's rule (only when the tooltip fits on neither side; default font widths) |
| Worn backpack model, upgrade particles, item colours | Done | `client/render` | Dropped backpacks and item frames show the 3D model with the ground / fixed transforms; vanilla slots (inventory, hotbar, containers) draw the two-colour layered icon; the smithing armour stand has no base plate (GX-1) |
| The sort key sorts without a click sound (only the Sort button clicks) | Done | `BackpackScreen.cpp` | |
| Crafting / smithing output preview: dyed colours, the source backpack (contents tooltip) | Done (GX-3) | `feature/tiers/CraftPreview.cpp` | |
| Clicking a backpack back into its slot places it (not equipped) | Done (GX-3) | `client/input/Stash.cpp`, `Hotkeys.cpp`, `SlotHover.cpp` | The main inventory grid's indices start at 0 (= slot 9); they collided with the hotbar |
| Diamond backpack durability workaround | Done (GX-3) | `feature/tiers/SmithingBase.cpp` | Removed: no durability |

## Strings

All the strings in scope have their English texts (config-screen and compat strings are N/A).
GX-4 added the Infinity / Survival Infinity tooltips and the three "already in another upgrade slot"
messages. Messages use "Backpack" for the leather tier. zh_CN (GX-5): every string uses the
established Simplified Chinese text where one exists (in the lang files and the resource pack
`texts.d`, e.g. 压制升级, 铜制背包, 空白升级); the others (e.g. the Stack Downgrade and Omega names and
tooltips) use our own translation.
