# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.0-beta.1] - 2026-11-11

First beta of 0.2.0. Please report anything that behaves differently from 0.1.2.

### Added

- Support for **Minecraft 1.26.60** with LeviLamina 26.60.x, which is now the version AlleviatedBackpacks is installed as by default. Minecraft 1.26.51 with LeviLamina 26.51.x is still supported and gets its own builds of every release, so nothing is lost by updating.
  - AlleviatedBackpacks is compiled against one LeviLamina line at a time, so each release now ships one download per line and side: `alleviatedbackpacks-{client,server}-win-x64-ll26.60.zip` and `alleviatedbackpacks-{client,server}-win-x64-ll26.51.zip`.

### Changed

- The package is now described by `lip.toml` of format 4 instead of `tooth.json`, so installing it needs [lip](https://github.com/LiteLDev/lip) **0.40.0** or later, or a LeviLauncher that reads it. Older releases stay installable with older lip.
- Each supported LeviLamina line is a variant named after it, with a server and a client build: `26_60` for 26.60.x and `26_51` for 26.51.x. The default variant (servers) and `client` (players) — the variants earlier releases were installed as — are the newest line, so `lip update` and LeviLauncher keep working and simply move you to 26.60.
  - To stay on Minecraft 1.26.51, install `github.com/MiracleForest/AlleviatedBackpacks#26_51`, or pick the `26_51` variant on the mod's page in LeviLauncher. lip keeps one variant of a mod at a time, so uninstall the one you have first (`lip uninstall github.com/MiracleForest/AlleviatedBackpacks[#client]`); your config stays.
  - LeviLamina 26.60 servers load mods from `mods/` instead of `plugins/`, so the server build of the 26.60 line goes to `mods/AlleviatedBackpacks/`, with its config in `mods/AlleviatedBackpacks/config/`. When you move a server from 26.51 to 26.60, copy your config over from `plugins/AlleviatedBackpacks/config/`. The `26_51` variant still installs into `plugins/AlleviatedBackpacks/`.
  - Your config in `mods/AlleviatedBackpacks/config/` (game) or `plugins/AlleviatedBackpacks/config/` (26.51 server) is kept on update.
- The downloads are now lip mod archives: the files sit at the root of the ZIP instead of inside an `AlleviatedBackpacks/` folder. Extract one into `mods/AlleviatedBackpacks/` (or `plugins/AlleviatedBackpacks/` on a 26.51 server), not into `mods/`.
- On Minecraft 1.26.60, the burn time of fuels for the Smelting, Smoking and Blasting upgrades comes from the game's fuel table and the item's `minecraft:fuel` component, because the call used for it on 1.26.51 is gone from 1.26.60. Nothing changes on 1.26.51.
- On Minecraft 1.26.60, the crafting and smithing result previews of backpack recipes find the screen's input slots by asking the game what each of the screen's collections is, because the lookup table used for it on 1.26.51 is gone from 1.26.60. Nothing changes on 1.26.51.

## [0.1.2] - 2026-11-01

### Changed

- The repository moved to [MiracleForest/AlleviatedBackpacks](https://github.com/MiracleForest/AlleviatedBackpacks); links point there now.
- The lip package is now `github.com/MiracleForest/AlleviatedBackpacks`, and its [LeviMC Mods page](https://levimc.org/mods/github.com/MiracleForest/AlleviatedBackpacks/) moves with it. Versions up to 0.1.1 still declare the old path: they stay installable at `github.com/LiteLDev/AlleviatedBackpacks`, and installing them by the new path needs lip 0.35.0 or later. With lip 0.36.0 or later, `lip update` moves an existing install to the new path by itself. With an older lip, run `lip uninstall github.com/LiteLDev/AlleviatedBackpacks[#client]` and then `lip install github.com/MiracleForest/AlleviatedBackpacks[#client]`. Either way the config in `plugins/AlleviatedBackpacks/config/` (server) or `mods/AlleviatedBackpacks/config/` (client) is kept.

## [0.1.1] - 2026-10-24

### Added

- A logo, and screenshots and a video of the backpacks and upgrades in the [LeviMC Mods gallery](https://levimc.org/mods/github.com/LiteLDev/AlleviatedBackpacks/?tab=gallery).
- [docs/ADMIN.md](docs/ADMIN.md): every command and config key, for server operators.

### Changed

- A shorter README written for players. Notes for developers (building, the energy and fluid API) moved to [CONTRIBUTING.md](CONTRIBUTING.md).

## [0.1.0] - 2026-10-24

First release, for Minecraft Bedrock Edition **1.26.51** and LeviLamina **26.51.7** (Windows x64).
There are two builds: `server` for BDS and `client` for the game. Every player needs the client
build, in single player and on a server.

### Added

- **Six backpack tiers**: Leather (27 storage / 1 upgrade slots), Copper (45/1), Iron (54/2),
  Gold (81/3), Diamond (108/5) and Netherite (120/7), with Sophisticated Backpacks' recipes, tier
  upgrades and dyeing.
  - Contents belong to the backpack, not to the player, and are stored in the world itself.
  - Backpacks can be worn, placed in the world, opened from another player's back and linked with
    the Ender Linker.
- **Sophisticated Backpacks' screens**, drawn by the client mod:
  - the storage screen with scroll bar, search (`@mod`, `#tooltip`, name), sort / sort-by,
    transfer buttons, abbreviated counts, memory and no-sort slots;
  - the upgrade column with on/off switches and an upgrade tab for every upgrade;
  - the settings screens (main, memory, no sort, item display, templates).
- **Every Sophisticated Backpacks upgrade**, basic and advanced: Pickup, Filter, Magnet, Feeding,
  Compacting, Void, Restock, Deposit, Refill, Tool Swapper, Jukebox, Pump, Alchemy, Mob Catcher;
  Stack (Starter, Tier 1-4, Omega, downgrades and conversion items); Crafting, Stonecutter, Anvil,
  Smithing; Smelting, Smoking, Blasting and their Auto versions; Inception, Everlasting, Infinity,
  Survival Infinity; Tank, Battery, XP Pump. Only the Chipped and Sawmill compat upgrades are
  missing (Java-only mods). Also the Hopper and Advanced Hopper upgrades from Sophisticated Storage.
- **Key bindings** (Settings > Keyboard & Mouse): open backpack, inventory interaction, tool swap,
  toggle upgrade 1-5, sort, transfer to storage / inventory, and right-click stashing.
- **Shift contents tooltip** on backpack items, and 3D backpack models worn, held, dropped, placed
  and in item frames.
- **World systems**: mobs spawning with backpacks, backpacks and upgrades in loot chests,
  duplicate backpack detection, piglins and the Gold Backpack, dispensers and hoppers.
- **Energy and fluid API** (FE and mB) for the Tank, Battery, Pump and XP Pump upgrades, public for
  other mods: see [docs/API.md](docs/API.md).
- **Configuration** with Sophisticated Backpacks' own config keys, names, defaults and ranges.
- **Commands**: `/backpack` (alias `/abp`) and `/sbp` (alias `/sophisticatedbackpacks`).
- English and Simplified Chinese; the GUI follows each player's language.

[Unreleased]: https://github.com/MiracleForest/AlleviatedBackpacks/compare/v0.2.0-beta.1...HEAD
[0.2.0-beta.1]: https://github.com/MiracleForest/AlleviatedBackpacks/compare/v0.1.2...v0.2.0-beta.1
[0.1.2]: https://github.com/MiracleForest/AlleviatedBackpacks/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/MiracleForest/AlleviatedBackpacks/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/MiracleForest/AlleviatedBackpacks/releases/tag/v0.1.0
