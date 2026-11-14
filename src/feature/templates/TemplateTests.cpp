// Selftests of the SNBT template files and the settings template packs.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

#include "mc/world/actor/player/Player.h"

#include "feature/templates/Snbt.h"
#include "feature/templates/Templates.h"
#include "selftest/SelfTest.h"

namespace abp::feature::templates {
namespace {
using selftest::Context;

/// JSON as it comes back from SNBT: null members left out.
nlohmann::json withoutNulls(nlohmann::json j) {
    if (j.is_object()) {
        for (auto it = j.begin(); it != j.end();) {
            if (it->is_null()) it = j.erase(it);
            else *it = withoutNulls(*it), ++it;
        }
    } else if (j.is_array()) {
        for (auto& e : j) e = e.is_null() ? nlohmann::json::object() : withoutNulls(e);
    }
    return j;
}

/// The pack folder `exportSettingsTemplate` writes to for `player`.
std::string exportFolderOf(Player& player) {
    std::string folder = player.getRealName();
    std::transform(folder.begin(), folder.end(), folder.begin(), [](unsigned char c) {
        return std::isspace(c) ? '_' : static_cast<char>(std::tolower(c));
    });
    return folder + "_abp_templates";
}

std::optional<nlohmann::json> packTemplate(std::string const& pack, std::string const& name) {
    auto const& all = packSettingsTemplates();
    auto        p   = all.find(pack);
    if (p == all.end()) return std::nullopt;
    auto t = p->second.find(name);
    if (t == p->second.end()) return std::nullopt;
    return t->second;
}

// GX-3: the mod exports templates as SNBT (NbtToSnbt / SnbtPrinterTagVisitor) and loads them with
// TagParser; earlier JSON exports still load.
ABP_REGISTER_SELFTEST("templates.snbt_files", [](Context& ctx) {
    nlohmann::json j = {
        {"flag",   true                                                       },
        {"off",    false                                                      },
        {"n",      42                                                         },
        {"big",    5000000000LL                                               },
        {"f",      1.5                                                        },
        {"s",      "Hello \"World\""                                          },
        {"ints",   {1, 2, 3}                                                  },
        {"mixed",  {1, 2.5}                                                   },
        {"strs",   nlohmann::json::array({"a", "b"})                              },
        {"slots",  {{{"id", "minecraft:stone"}, {"count", 3}}, nullptr}       },
        {"other",  {1, "x"}                                                   },
        {"empty",  nlohmann::json::array()                                    },
        {"nested", {{"memory", {{"slotFilterItems", {{"3", "minecraft:dirt"}}}}}}},
        {"gone",   nullptr                                                    },
    };
    auto snbt = jsonToSnbt(j);
    ctx.require(snbt.has_value(), "written");
    ctx.info("snbt: " + *snbt);
    ctx.check(snbt->find("1b") != std::string::npos && (snbt->find("5000000000L") != std::string::npos || snbt->find("5000000000l") != std::string::npos), "NBT types (byte / long) in the text");
    auto back = snbtToJson(*snbt);
    ctx.require(back.has_value(), "parsed back");
    auto expect = j;
    expect.erase("gone");                                 // null members are left out
    expect["mixed"]    = {1.0, 2.5};                       // one numeric type per list
    expect["slots"][1] = nlohmann::json::object();         // a null element is an empty compound
    ctx.check(*back == expect, "round trip: " + back->dump());
    ctx.check(!snbtToJson("{not snbt").has_value(), "invalid text refused");

    // Files: an export writes .snbt (and replaces an older .json of the same template); both load.
    namespace fs = std::filesystem;
    auto& player = ctx.player();
    auto  folder = exportFolderOf(player);
    auto  root   = templatePacksRoot() / folder;
    auto  dir    = root / "data" / folder / "abp_settingstemplates";
    std::error_code ec;
    fs::create_directories(dir, ec);
    nlohmann::json settings = {
        {"memory", {{"slotFilterItems", {{"3", "minecraft:dirt"}}}, {"gone", nullptr}}},
        {"nosort", {{"color", 3}}                                                  },
    };
    {
        std::ofstream legacy(dir / "old_kit.json");
        legacy << settings.dump(4);
        std::ofstream stale(dir / "kit.json");
        stale << settings.dump(4);
    }
    auto path = exportSettingsTemplate(player, "kit", settings);
    ctx.require(path.has_value() && path->ends_with("kit.snbt"), "exported as .snbt: " + path.value_or("(none)"));
    ctx.check(fs::exists(dir / "kit.snbt") && !fs::exists(dir / "kit.json"), "the older .json export is replaced");
    auto beforeReload = packTemplate(folder, "Kit");
    reloadTemplatePacks();
    auto kit = packTemplate(folder, "Kit");
    auto old = packTemplate(folder, "Old Kit");
    ctx.check(kit.has_value() && *kit == withoutNulls(settings), "SNBT template reads back the same");
    ctx.check(beforeReload && kit && *beforeReload == *kit, "the exported template is the same before and after a reload");
    ctx.check(old.has_value() && *old == settings, "legacy JSON template still loads");
    fs::remove_all(root, ec);
    reloadTemplatePacks();
});

// EXPORT_FILE_NAME_PATTERN `[A-Za-z0-9/._\-\s]+`: `/` and `.` are allowed (sub-folders of
// the templates folder); a name leading out of that folder is refused.
ABP_REGISTER_SELFTEST("templates.export_names", [](Context& ctx) {
    ctx.check(invalidFileNameCharacters("my kit_2-b").empty(), "letters, digits, space, _ and - allowed");
    ctx.check(invalidFileNameCharacters("mine/v1.2").empty(), "/ and . allowed");
    ctx.check(invalidFileNameCharacters("a:b*c\\d") == ":*\\", "other characters listed: " + invalidFileNameCharacters("a:b*c\\d"));

    namespace fs = std::filesystem;
    auto& player = ctx.player();
    auto  folder = exportFolderOf(player);
    auto  root   = templatePacksRoot() / folder;
    auto  dir    = root / "data" / folder / "abp_settingstemplates";
    nlohmann::json settings = {{"nosort", {{"color", 3}}}};
    auto sub = exportSettingsTemplate(player, "mine/kit", settings);
    ctx.check(sub.has_value() && fs::exists(dir / "mine" / "kit.snbt"), "sub-folder export: " + sub.value_or("(none)"));
    ctx.check(!exportSettingsTemplate(player, "../../escape", settings).has_value(), "export outside the templates folder refused");
    ctx.check(!fs::exists(root / "data" / "escape.snbt"), "nothing written outside");
    reloadTemplatePacks();
    ctx.check(packTemplate(folder, "Kit").has_value(), "sub-folder template loads under its file name");
    std::error_code ec;
    fs::remove_all(root, ec);
    reloadTemplatePacks();
});

} // namespace
} // namespace abp::feature::templates
