#pragma once

// P2-2 - Settings templates (SettingsTemplateStorage /
// DatapackSettingsTemplateManager).
//
// Persistence:
// - Player settings templates live in the world's LevelDB (key `abp:settings_templates`), like
//   SettingsTemplateStorage (SavedData in the overworld).
// - Bedrock has no data packs. Their equivalent is the "template packs" folder
//   `<mod>/config/templates/`, laid out like exported data packs:
//     <pack>/data/<namespace>/abp_settingstemplates/<name>.snbt
//   SNBT (Snbt.h); `.json` files of earlier exports are still read. It is read when the
//   mod enables and on `/backpack reload`; exports (settings screen "Export") are written there as
//   `.snbt` and registered at once.

#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "nlohmann/json.hpp"

class Player;

namespace abp::feature::templates {

// ---- template packs folder ------------------------------------------------------------------------
[[nodiscard]] std::filesystem::path templatePacksRoot();
/// Re-reads every template pack.
void reloadTemplatePacks();

// ---- settings templates ------------------------------------------------------------------------------
[[nodiscard]] std::map<int, nlohmann::json>         playerTemplates(Player& player);
[[nodiscard]] std::map<std::string, nlohmann::json> playerNamedTemplates(Player& player);
void putPlayerTemplate(Player& player, int slot, nlohmann::json settings);
void putPlayerNamedTemplate(Player& player, std::string const& name, nlohmann::json settings);
void clearPlayerTemplates(Player& player);

/// pack name -> template name ("Capitalized Words") -> settings, both sorted.
using PackTemplates = std::map<std::string, std::map<std::string, nlohmann::json>>;
[[nodiscard]] PackTemplates const& packSettingsTemplates();
/// DatapackSettingsTemplateManager.putTemplate ('_' -> ' ', words capitalized).
void putPackSettingsTemplate(std::string const& pack, std::string const& fileName, nlohmann::json settings);

/// TemplatePersistanceContainer.exportTemplate after the name checks: writes
/// `<root>/<player>_abp_templates/data/<player>_abp_templates/abp_settingstemplates/<file>.snbt`
/// (`fileName` may contain `/` sub-folders, as the name pattern allows) and registers it.
/// Returns the written path relative to the packs root; nullopt on I/O errors or when the name would
/// place the file outside that `abp_settingstemplates` folder (e.g. `../x`).
[[nodiscard]] std::optional<std::string> exportSettingsTemplate(Player& player, std::string const& fileName, nlohmann::json const& settings);

/// EXPORT_FILE_NAME_PATTERN `[A-Za-z0-9/._\-\s]+` (letters, digits, `/`, `.`, `_`, `-`,
/// whitespace): the characters of `name` that don't match.
[[nodiscard]] std::string invalidFileNameCharacters(std::string const& name);

} // namespace abp::feature::templates
