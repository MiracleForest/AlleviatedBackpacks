# Contributing to AlleviatedBackpacks

Thanks for helping! Bug reports, ideas and pull requests are all welcome.

## Reporting bugs and requesting features

Use the [issue templates](https://github.com/MiracleForest/AlleviatedBackpacks/issues/new/choose). For bugs, include:

- the AlleviatedBackpacks version;
- the LeviLamina version;
- the Minecraft version;
- single player or server, and whether the player runs the client mod;
- steps to reproduce;
- the relevant part of `logs/latest.log` from your game or server folder.

## Development setup

See [Development](#development) below. [AGENTS.md](AGENTS.md) is the developer guide. It covers the layout of the code, the feature-unit rules, the contracts between modules, the client companion and the Bedrock / LeviLamina rules. Please read it before changing anything under `src/`.

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Keep each pull request focused on one change. Describe what it does and how you tested it.
3. Follow the existing style: C++20, the provided `.clang-format` and `.editorconfig`, English in code and comments.
4. User-facing text goes through `abp::tr` / `abp::trFor` with entries in **both** `assets/lang/en_US/` and `assets/lang/zh_CN/` (and the resource pack's `texts.d/`).
5. Logic in `src/core` must stay free of game headers and come with unit tests in `tests/`:
   `xmake build -y abp-tests; xmake run abp-tests`. Features come with in-game selftests.
6. Both builds must compile: `pwsh tools/build.ps1 -Target all`.
7. If you change something visible in game, run the relevant end-to-end scenario from [tools/e2e](tools/e2e/README.md) and check its screenshots. Better still, add a scenario.
8. CI must pass before a pull request is merged.

Commit messages use the imperative mood ("Add jukebox tab", not "Added …"). Update [CHANGELOG.md](CHANGELOG.md) under *Unreleased* for user-visible changes.

Never copy code or assets from the Java Sophisticated Backpacks repository: it is a behaviour reference only.

## Development

### Building

You need Windows x64, [xmake](https://xmake.io), Visual Studio Build Tools with MSVC 14.4x or newer, and LLVM (`clang-cl`).

AlleviatedBackpacks is compiled against one LeviLamina line at a time. Each supported line has a lip variant of the same name and a lock file in `locks/`, which pins the packages of that line; `tools/build.ps1 -Line <variant>` copies it to `xmake-requires.lock` (which is generated, and not tracked) and configures with that line's `--levilamina`:

| Line | Variant | Lock file | `--levilamina` |
| --- | --- | --- | --- |
| LeviLamina 26.60.x (Minecraft 1.26.60) | `26_60` (the default) | `locks/26_60.lock` | `26.60.6` (the default) |
| LeviLamina 26.51.x (Minecraft 1.26.51) | `26_51` | `locks/26_51.lock` | `26.51.7` |

```powershell
git config --global core.longpaths true
pwsh tools/build.ps1 -Target all -Tests     # or -Target server / -Target client
pwsh tools/build.ps1 -Line 26_51            # the other line
```

Outputs: `bin/server/AlleviatedBackpacks/` (copy to `<BDS>/mods/`, or to `<BDS>/plugins/` for
LeviLamina 26.51) and `bin/client/AlleviatedBackpacks/` (copy to
`%APPDATA%\levilauncher.exe\versions\<version>\mods\`).
There are two builds from one source: `server` for BDS and `client` for the game. The client
build is the only backpack UI: it draws Sophisticated Backpacks' screens, adds the key bindings and
the Shift contents tooltip, while every item move is still done by the server.

The build defines `ABP_LL_VERSION` as the line it is building for (`2651`, `2660`). Where a LeviLamina API differs between lines, guard both paths with it rather than dropping the older one, and keep the guard as small as possible:

```cpp
#if ABP_LL_VERSION >= 2660
    // 26.60 and later
#else
    // 26.51
#endif
```

To add a line, add its row above: a `locks/<variant>.lock` (configure once with the new `--levilamina` for both `--target_type`s and copy the generated `xmake-requires.lock`), its version in `tools/build.ps1`, a variant in `lip.toml`, and a matrix entry in `.github/workflows/build.yml` and `.github/workflows/release.yml`.

### Releasing

The version is written in three places and the release workflow refuses a tag that disagrees with
any of them:

1. `lip.toml` — `version`;
2. `manifest.json` — `version`, what the mod reports in game (the packs take its `major.minor.patch`);
3. `CHANGELOG.md` — a `## [<version>] - <date>` section, which becomes the release notes.

Then tag the commit `v<version>` and push the tag. The workflow builds every line for the server
and the client, runs the unit tests, packs one ZIP per line and side
(`alleviatedbackpacks-{server,client}-win-x64-ll<line>.zip`, `manifest.json` at the root) and
creates the release; a version with a `-` in it, such as `0.2.0-beta.1`, is published as a
prerelease.

### Tests

- Unit tests for `src/core`: `xmake build -y abp-tests; xmake run abp-tests`.
- In-game selftests: `/backpack selftest ["glob"]` (operators / console).
- End-to-end tests that drive the real server and game: [tools/e2e/README.md](tools/e2e/README.md).

### Showcase media

The screenshots and videos in `docs/showcase/` (README and the
[LeviMC Mods gallery](https://levimc.org/mods/github.com/MiracleForest/AlleviatedBackpacks/?tab=gallery),
listed in `docs/showcase/showcase.json`) come from the client scenario `showcase`:

```powershell
pwsh -NoProfile -File tools/e2e/client/run.ps1 -Scenario showcase
pwsh tools/e2e/client/showcase-encode.ps1      # client/out/showcase -> docs/showcase (needs ffmpeg)
```

### How it works

- [AGENTS.md](AGENTS.md): code layout, feature units, contracts between modules, the client
  companion and the Bedrock / LeviLamina rules.
- [docs/API.md](docs/API.md): the energy (FE) and fluid (mB) API that the Tank, Battery, Pump and
  XP Pump upgrades use, public for other mods.
- [docs/FEATURES.md](docs/FEATURES.md): feature-by-feature comparison with Sophisticated Backpacks.
- [docs/ADMIN.md](docs/ADMIN.md): every command and config file, for server operators.

## License

By contributing you agree that your contributions are licensed under the [LGPL-3.0-or-later](COPYING.LESSER), the license of this project.
