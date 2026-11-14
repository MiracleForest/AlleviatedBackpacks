// Settings templates and template packs. See Templates.h.

#include "feature/templates/Templates.h"
#include "feature/templates/Snbt.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/server/ServerStoppingEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/platform/UUID.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelStorage.h"
#include "mc/world/level/storage/db_helpers/Category.h"

#include "mod/Config.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace abp::feature::templates {

namespace {
using nlohmann::json;
namespace fs = std::filesystem;

constexpr char const* kSettingsKey = "abp:settings_templates";
constexpr char const* kSettingsDir = "abp_settingstemplates";

// ---- world DB blobs ---------------------------------------------------------------------------------
LevelStorage* levelStorage() {
    auto level = ll::service::getLevel();
    return level ? &level->getLevelStorage() : nullptr;
}

struct WorldBlob {
    char const* key;
    bool        loaded = false;
    json        value  = json::object();

    json& get() {
        if (loaded) return value;
        loaded = true;
        value  = json::object();
        if (auto* db = levelStorage()) {
            std::string buffer;
            if (db->loadData(key, buffer, DBHelpers::Category::Uncategorized)) {
                auto parsed = json::parse(buffer, nullptr, false);
                if (parsed.is_object()) value = std::move(parsed);
                else log().warn("templates: {} unreadable; starting empty", key);
            }
        }
        return value;
    }
    void save() {
        if (auto* db = levelStorage()) db->saveData(key, value.dump(), DBHelpers::Category::Uncategorized);
    }
    void reset() {
        loaded = false;
        value  = json::object();
    }
};
WorldBlob gSettingsBlob{kSettingsKey};

std::string uuidOf(Player& p) { return p.getUuid().asString(); }

// ---- template packs -------------------------------------------------------------------------------------
PackTemplates                         gPackSettings;

std::string capitalizeWords(std::string name) {
    std::replace(name.begin(), name.end(), '_', ' ');
    std::string out, word;
    std::istringstream in(name);
    while (in >> word) {
        word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
        out += (out.empty() ? "" : " ") + word;
    }
    return out;
}

std::optional<json> readJsonFile(fs::path const& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::stringstream ss;
    ss << in.rdbuf();
    auto j = json::parse(ss.str(), nullptr, false, true);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;
    return j;
}

/// A template file: `.snbt` (DatapackSettingsTemplateManager.Loader parses these with
/// TagParser), or the `.json` files of earlier exports.
std::optional<json> readTemplateFile(fs::path const& path) {
    if (path.extension() == ".json") return readJsonFile(path);
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::stringstream ss;
    ss << in.rdbuf();
    auto j = snbtToJson(ss.str());
    if (!j || !j->is_object()) return std::nullopt;
    return j;
}

bool isTemplateFile(fs::path const& path) { return path.extension() == ".snbt" || path.extension() == ".json"; }

/// A template as it comes back from its `.snbt` file (null members dropped, one type per list).
json asReadBack(json const& j) {
    auto tag = jsonToTag(j);
    return tag ? tagToJson(*tag) : j;
}

bool writeJsonFile(fs::path const& path, json const& j) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
        log().error("templates: cannot create {}: {}", path.parent_path().string(), ec.message());
        return false;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        log().error("templates: cannot write {}", path.string());
        return false;
    }
    out << j.dump(4) << '\n';
    return static_cast<bool>(out);
}

/// exportTemplate (NbtToSnbt.writeSnbt): the template as SNBT. An older `.json`
/// export of the same template is removed, so the new file is the only one.
bool writeSnbtFile(fs::path const& path, json const& j) {
    auto snbt = jsonToSnbt(j);
    if (!snbt) {
        log().error("templates: {} is not a compound", path.string());
        return false;
    }
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
        log().error("templates: cannot create {}: {}", path.parent_path().string(), ec.message());
        return false;
    }
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            log().error("templates: cannot write {}", path.string());
            return false;
        }
        out << *snbt << '\n';
        if (!out) return false;
    }
    auto legacy = path;
    legacy.replace_extension(".json");
    fs::remove(legacy, ec);
    return true;
}

/// initDatapackStructure writes pack.mcmeta into a new data pack; the equivalent marks the
/// folder as a template pack.
void writePackMeta(fs::path const& packRoot, std::string const& description) {
    auto meta = packRoot / "pack.json";
    if (fs::exists(meta)) return;
    (void)writeJsonFile(meta, json{{"pack", {{"description", description}}}});
}

std::string packRelative(fs::path const& p) {
    std::error_code ec;
    auto            rel = fs::relative(p, templatePacksRoot().parent_path(), ec);
    return (ec ? p : rel).generic_string();
}

void loadPacks() {
    gPackSettings.clear();
    auto            root = templatePacksRoot();
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return;
    int settings = 0;
    for (auto const& pack : fs::directory_iterator(root, ec)) {
        auto data = pack.path() / "data";
        if (!pack.is_directory() || !fs::is_directory(data, ec)) continue;
        for (auto const& ns : fs::directory_iterator(data, ec)) {
            if (!ns.is_directory()) continue;
            auto nsName = ns.path().filename().string();
            auto base = ns.path() / kSettingsDir;
            if (!fs::is_directory(base, ec)) continue;
            for (auto const& f : fs::recursive_directory_iterator(base, ec)) {
                if (!f.is_regular_file() || !isTemplateFile(f.path())) continue;
                auto j = readTemplateFile(f.path());
                if (!j) {
                    log().error("templates: couldn't parse template file {}", f.path().string());
                    continue;
                }
                putPackSettingsTemplate(nsName, f.path().stem().string(), *j);
                ++settings;
            }
        }
    }
    log().info("templates: loaded {} settings templates from {}", settings, root.string());
}

class TemplatesModule {
public:
    void install() {
        loadPacks();
        static bool reloadHooked = false;
        if (!reloadHooked) {
            reloadHooked = true;
            config::onReload([] { loadPacks(); });
        }
        mStopping = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerStoppingEvent>([](auto&) {
            gSettingsBlob.reset();
        });
    }
    void uninstall() {
        ll::event::EventBus::getInstance().removeListener(mStopping);
        gSettingsBlob.reset();
    }

private:
    ll::event::ListenerPtr mStopping;
};
} // namespace

// ---- template packs ------------------------------------------------------------------------------------

std::filesystem::path templatePacksRoot() { return config::configDir() / "templates"; }

void reloadTemplatePacks() { loadPacks(); }

// ---- settings templates ---------------------------------------------------------------------------------

std::map<int, nlohmann::json> playerTemplates(Player& player) {
    std::map<int, json> out;
    auto& all = gSettingsBlob.get();
    auto  pt  = all.find("playerTemplates");
    if (pt == all.end() || !pt->is_object()) return out;
    auto mine = pt->find(uuidOf(player));
    if (mine == pt->end() || !mine->is_object()) return out;
    for (auto const& [k, v] : mine->items()) {
        try {
            out[std::stoi(k)] = v;
        } catch (...) {
        }
    }
    return out;
}

std::map<std::string, nlohmann::json> playerNamedTemplates(Player& player) {
    std::map<std::string, json> out;
    auto& all = gSettingsBlob.get();
    auto  pt  = all.find("playerNamedTemplates");
    if (pt == all.end() || !pt->is_object()) return out;
    auto mine = pt->find(uuidOf(player));
    if (mine == pt->end() || !mine->is_object()) return out;
    for (auto const& [k, v] : mine->items()) out[k] = v;
    return out;
}

void putPlayerTemplate(Player& player, int slot, nlohmann::json settings) {
    gSettingsBlob.get()["playerTemplates"][uuidOf(player)][std::to_string(slot)] = std::move(settings);
    gSettingsBlob.save();
}

void putPlayerNamedTemplate(Player& player, std::string const& name, nlohmann::json settings) {
    gSettingsBlob.get()["playerNamedTemplates"][uuidOf(player)][name] = std::move(settings);
    gSettingsBlob.save();
}

void clearPlayerTemplates(Player& player) {
    auto& all = gSettingsBlob.get();
    for (auto const* key : {"playerTemplates", "playerNamedTemplates"}) {
        if (auto it = all.find(key); it != all.end() && it->is_object()) it->erase(uuidOf(player));
    }
    gSettingsBlob.save();
}

PackTemplates const& packSettingsTemplates() { return gPackSettings; }

void putPackSettingsTemplate(std::string const& pack, std::string const& fileName, nlohmann::json settings) {
    gPackSettings[pack][capitalizeWords(fileName)] = std::move(settings);
}

std::string invalidFileNameCharacters(std::string const& name) {
    std::string bad;
    for (unsigned char c : name) {
        bool ascii = c < 0x80;
        if (ascii && (std::isalnum(c) || std::isspace(c))) continue;
        if (c == '/' || c == '.' || c == '_' || c == '-') continue;
        bad += static_cast<char>(c);
    }
    return bad;
}

std::optional<std::string> exportSettingsTemplate(Player& player, std::string const& fileName, nlohmann::json const& settings) {
    std::string folder = player.getRealName();
    std::transform(folder.begin(), folder.end(), folder.begin(), [](unsigned char c) {
        return std::isspace(c) ? '_' : static_cast<char>(std::tolower(c));
    });
    folder += "_abp_templates";
    auto packRoot = templatePacksRoot() / folder;
    auto base     = (packRoot / "data" / folder / kSettingsDir).lexically_normal();
    auto file     = (base / std::filesystem::path(fileName + ".snbt")).lexically_normal();
    // The name may hold `/` and `.` (sub-folders); it must not lead out of the templates folder.
    if (auto rel = file.lexically_relative(base); rel.empty() || *rel.begin() == std::filesystem::path("..")) {
        log().error("templates: export name '{}' points outside {}", fileName, base.string());
        return std::nullopt;
    }
    writePackMeta(packRoot, "Settings templates data pack");
    if (!writeSnbtFile(file, settings)) return std::nullopt;
    putPackSettingsTemplate(folder, fileName, asReadBack(settings)); // what a reload will see
    return packRelative(file);
}

} // namespace abp::feature::templates

ABP_REGISTER_MODULE(abp::feature::templates::TemplatesModule);
