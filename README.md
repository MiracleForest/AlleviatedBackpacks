# AlleviatedBackpacks

<img src="docs/logo.png" alt="" width="128" align="right">

[![Release](https://img.shields.io/github/v/release/MiracleForest/AlleviatedBackpacks)](https://github.com/MiracleForest/AlleviatedBackpacks/releases)
[![License](https://img.shields.io/github/license/MiracleForest/AlleviatedBackpacks)](COPYING.LESSER)

Backpacks with upgrades for **Minecraft Bedrock Edition**: the gameplay of the Java mod
[Sophisticated Backpacks](https://www.curseforge.com/minecraft/mc-mods/sophisticated-backpacks),
in single player and on servers. Wear them, place them, fill them, and slot in upgrades that pick
up, feed, craft and sort for you.

![Six backpack tiers placed on the grass: leather, copper, iron, gold, diamond and netherite](docs/showcase/tiers-placed.webp)

## Features

- **Six tiers**, from Leather to Netherite. Bigger tiers hold more items and more upgrades.

  | Tier | Leather | Copper | Iron | Gold | Diamond | Netherite |
  | ---- | :-----: | :----: | :--: | :--: | :-----: | :-------: |
  | Storage slots | 27 | 45 | 54 | 81 | 108 | 120 |
  | Upgrade slots | 1 | 1 | 2 | 3 | 5 | 7 |

- **Sophisticated Backpacks' own screen**: a scroll bar, search, sorting, quick transfer buttons,
  and an upgrade column with on/off switches and a settings tab for every upgrade.

  ![The Diamond Backpack screen, full of items, with crafting, magnet, feeding, pickup and smelting upgrades](docs/showcase/backpack-screen.webp)

- **Every upgrade from the Java mod**: Pickup, Magnet, Feeding, Filter, Compacting, Void, Restock,
  Deposit, Refill, Tool Swapper, Crafting, Stonecutter, Anvil, Smithing, Smelting / Smoking /
  Blasting (and auto versions), Stack, Jukebox, Tank, Battery, Pump, XP Pump, Alchemy,
  Mob Catcher, Inception, Everlasting and Infinity. Most come in a basic and an advanced version
  with item filters. The Ender Linker links backpacks so they share their contents.

  ![The Advanced Magnet upgrade switched on with Alt+Z: the drops around the player fly into the worn backpack](docs/showcase/magnet-anim.webp)

- **Contents belong to the backpack**: drop it, hand it to a friend, put it in a chest, place it on
  the ground. The items stay inside. Shift over a backpack to peek at what it holds.
- **Worn on your back, held, or placed** as a block, with a 3D model. Mobs sometimes spawn wearing
  backpacks, and dungeon chests can hold backpacks and upgrades.
- In-game text in English and Simplified Chinese; each player sees their own game language.

## Install

Players need the **client** mod to open backpacks, in single player and on a server. A server
needs the **server** mod. Use the same version on both.

A release is built against each LeviLamina line it supports; pick the one that matches your game
or server:

| Minecraft | LeviLamina | Variant | Downloads |
| --------- | ---------- | ------- | --------- |
| 1.26.60 | 26.60.x | `26_60`, and the default | `alleviatedbackpacks-{client,server}-win-x64-ll26.60.zip` |
| 1.26.51 | 26.51.x | `26_51` | `alleviatedbackpacks-{client,server}-win-x64-ll26.51.zip` |

In LeviLauncher, install AlleviatedBackpacks from the **LeviMC Mods** page; it offers the variants
above, and the default is the newest line.

On a dedicated server, use [lip](https://github.com/LiteLDev/lip) 0.40.0 or later (lip manages
servers only; in the game, use LeviLauncher):

```sh
lip install github.com/MiracleForest/AlleviatedBackpacks          # the newest line (26.60)
lip install github.com/MiracleForest/AlleviatedBackpacks#26_51    # Minecraft 1.26.51
```

lip keeps one variant of a mod at a time: to switch variants, `lip uninstall` the installed one
first (the config is kept).

Or download the ZIPs for your line from the
[latest release](https://github.com/MiracleForest/AlleviatedBackpacks/releases/latest). The files
sit at the root of each ZIP:

- **Players:** `alleviatedbackpacks-client-win-x64-ll<line>.zip`. Extract it into an
  `AlleviatedBackpacks` folder inside the game version's `mods` folder, so that
  `mods/AlleviatedBackpacks/manifest.json` exists. Every local world gets the backpacks; nothing
  to switch on.
- **Servers:** `alleviatedbackpacks-server-win-x64-ll<line>.zip`, extracted into
  `mods/AlleviatedBackpacks/` on a LeviLamina 26.60 server, or into `plugins/AlleviatedBackpacks/`
  on a LeviLamina 26.51 server (26.51 servers load mods from `plugins`). The server hands its
  resource pack to joining players.

Requires Minecraft Bedrock Edition **1.26.60** with LeviLamina **26.60.x**, or **1.26.51** with
LeviLamina **26.51.x** (Windows x64). Players without the client mod can still join a server and
carry backpacks, but cannot open them: they get a download link instead of the backpack screen.

## How to use

**Crafting.** Every recipe is in the recipe book; search for "backpack" or "upgrade".

![The Leather Backpack recipe in a crafting table](docs/showcase/recipe.webp)

- **Leather Backpack**: chest in the middle, leather and string around it.
- **Next tier**: surround a backpack with copper / iron / gold ingots or diamonds; a Netherite
  Backpack is made at the smithing table. The contents and upgrades come along.
- **Upgrades** start from an **Upgrade Base** (string, iron, leather). Dye backpacks in a crafting
  table.

**Opening.** Right-click with a backpack in hand, press **B** to open the one you carry or wear, or
right-click a placed backpack. Sneak + right-click places a backpack; sneak + right-click with an
empty hand picks it up again. Put an upgrade into one of the slots on the left of the screen and
click its tab on the right to set it up.

| Key (Settings > Keyboard & Mouse) | Default | What it does |
| --------------------------------- | ------- | ------------ |
| Open Backpack | `B` | open the backpack you carry or wear (also over a backpack in any inventory) |
| Inventory Interaction | `C` | deposit into / restock from the container you look at |
| Toggle Upgrade 1 / 2 | `Alt+Z` / `Alt+X` | switch the first / second upgrade on or off (3–5 unbound) |
| Tool Swap | unbound | swap to the right tool (Tool Swapper upgrade) |
| Sort | middle mouse | sort the backpack |
| Transfer to Storage / Inventory | `[` / `]` | move items into / out of the backpack (Shift: everything) |

Commands: `/backpack open` opens the backpack in your hand; `/backpack info` shows its tier and
upgrades.

## Settings

The config files are in the mod's `config` folder (`mods/AlleviatedBackpacks/config/` in the game
and on a 26.60 server, `plugins/AlleviatedBackpacks/config/` on a 26.51 server), created on first
start. `/backpack reload`
applies changes.

- `general.json`: `allowNesting` (backpacks in backpacks without the Inception upgrade),
  `allowShulkerBoxes`, `usePlayerLocale` (messages in each player's language).
- `sophisticatedbackpacks-server.json`: the Java mod's own settings with the same names, such as
  slot counts per tier, disallowed items and whether players may open other players' backpacks.
- `<upgrade>.json`: each upgrade's ranges, filter slots and speeds (for example `magnet.json`).
- `client_gui.json` (game only): `sortButtonsPosition`, `playButtonSound`.

Running a server? Every command and config key is listed in [docs/ADMIN.md](docs/ADMIN.md).

## More

More screenshots and videos: https://levimc.org/mods/github.com/MiracleForest/AlleviatedBackpacks/?tab=gallery

Building it yourself, or writing a mod against its energy and fluid API? See
[CONTRIBUTING.md](CONTRIBUTING.md).

Inspired by Sophisticated Backpacks by P3pp3rF1y. This is an independent implementation for
Bedrock, not affiliated with Mojang Studios, Microsoft or the Sophisticated Backpacks project.

## License

[LGPL-3.0-or-later](COPYING.LESSER) © LeviMC
