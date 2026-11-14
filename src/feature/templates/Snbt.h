#pragma once

// GX-3: settings template files as SNBT, the format the mod uses for them
// (`abp_settingstemplates/*.snbt`: written by TemplatePersistanceContainer.exportTemplate
// through NbtToSnbt / SnbtPrinterTagVisitor, read by DatapackSettingsTemplateManager with TagParser).
//
// Our template data is JSON in memory; it is written as the equivalent compound tag:
//   object -> compound (null members are left out), array -> list, string -> string,
//   bool -> byte (0b / 1b), integer -> int (long when it does not fit), float -> double.
// Lists in NBT hold one tag type: integer lists that need a long become long lists, numbers mixed
// with floats become double lists, a null element is an empty compound, and any other mix is
// stored as a list of `{"": element}` compounds (unwrapped again on reading).
// Reading maps back: byte 0/1 -> bool, other numbers -> numbers, byte / int arrays -> arrays.

#include <optional>
#include <string>
#include <string_view>

#include "nlohmann/json.hpp"

class CompoundTag;

namespace abp::feature::templates {

/// The compound tag of a JSON object (nullopt when `j` is not an object).
[[nodiscard]] std::optional<CompoundTag> jsonToTag(nlohmann::json const& j);
/// The JSON object of a compound tag.
[[nodiscard]] nlohmann::json tagToJson(CompoundTag const& tag);

/// Pretty SNBT (indented with 4 spaces, like SnbtPrinterTagVisitor output)
/// of a JSON object.
[[nodiscard]] std::optional<std::string> jsonToSnbt(nlohmann::json const& j);
/// Parses SNBT text (a compound) into JSON; nullopt when it is not valid SNBT.
[[nodiscard]] std::optional<nlohmann::json> snbtToJson(std::string_view snbt);

} // namespace abp::feature::templates
