# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-11-14

First release, for Minecraft Bedrock Edition **1.26.60** with LeviLamina **26.60.x** and
**1.26.51** with LeviLamina **26.51.x** (Windows x64). Each release has a server build (BDS) and a
client build (the game) per line: `alleviatedbackpacks-{server,client}-win-x64-ll26.60.zip` and
`alleviatedbackpacks-{server,client}-win-x64-ll26.51.zip`. Every player needs the client build, in
single player and on a server.

### Added

- **Six backpack tiers**: Leather (27 storage / 1 upgrade slots), Copper (45/1), Iron (54/2),
  Gold (81/3), Diamond (108/5) and Netherite (120/7), with recipes, tier upgrades that keep the
  contents, and dyeing.
  - Contents belong to the backpack, not to the player, and are stored in the world itself.
  - Backpacks can be worn, held, placed in the world as a block, and opened from another player's back.
- **Backpack screens**:
  - the storage screen with scroll bar, search (`@mod`, `#tooltip`, name), sort / sort-by,
    transfer buttons, abbreviated counts, memory and no-sort slots;
  - the upgrade column with on/off switches and a settings tab for every upgrade;
  - the settings screens (main, memory, no sort, item display, settings templates).
- **Upgrades**, most in a basic and an advanced version with item filters: Pickup, Filter, Magnet,
  Feeding, Compacting, Void, Restock, Deposit, Refill, Tool Swapper, Jukebox, Pump; Stack (Starter,
  Tier 1-4, Omega and downgrades); Crafting, Stonecutter, Anvil, Smithing; Smelting, Smoking,
  Blasting and their Auto versions; Inception, Everlasting, Infinity, Survival Infinity; Tank,
  Battery, XP Pump; Hopper and Advanced Hopper.
- **Key bindings** (Settings > Keyboard & Mouse): open backpack, inventory interaction, tool swap,
  toggle upgrade 1-5, sort, transfer to storage / inventory, and right-click stashing.
- **Shift contents tooltip** on backpack items, and 3D backpack models worn, held, dropped, placed
  and in item frames.
- **World systems**: mobs spawning with backpacks, backpacks and upgrades in loot chests,
  duplicate backpack detection, piglins and the Gold Backpack, dispensers and hoppers.
- **Energy and fluid API** (FE and mB) for the Tank, Battery, Pump and XP Pump upgrades, public for
  other mods: see [docs/API.md](docs/API.md).
- **Configuration** of slot counts, upgrades, world systems and more, with documented defaults and
  ranges; every command and config key is listed in [docs/ADMIN.md](docs/ADMIN.md).
- **Commands**: `/backpack` for players and operators, and `/backpackadmin` for operators.
- English and Simplified Chinese; the GUI follows each player's language.
- Installable with [lip](https://github.com/LiteLDev/lip) 0.40.0 or later on servers and from the
  LeviMC Mods page in LeviLauncher, with a variant per LeviLamina line (`26_60`, the default, and
  `26_51`).

[Unreleased]: https://github.com/MiracleForest/AlleviatedBackpacks/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/MiracleForest/AlleviatedBackpacks/releases/tag/v0.1.0
