// `/backpackadmin`, the admin command
// (ListCommand, GiveCommand, RemoveNonPlayerCommand):
//
//   /backpackadmin list [playerName]                          access log, newest first
//   /backpackadmin give <targets> <backpackUuid>              a backpack item bound to a logged backpack
//   /backpackadmin removeNonPlayer <onlyWithEmptyInventory>   delete backpacks no player ever opened
//
// Operators only (op level 2 = Bedrock operators). The same overloads also work as
// `/backpack list|give|removeNonPlayer`. Bedrock chat has no click / hover events: the uuid and
// player name parameters autocomplete from the access log instead (the argument types
// BackpackUUIDArgumentType / BackpackPlayerArgumentType).

#include "feature/admin/AbpAdmin.h"

#include <algorithm>
#include <ctime>
#include <set>

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/SoftEnum.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/command/ServerCommandRegisterEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/server/commands/CommandSelector.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/safety/RedactableString.h"

#include "core/WorldRules.h"
#include "feature/admin/AccessLog.h"
#include "feature/admin/Admin.h"
#include "game/BackpackItem.h"
#include "game/ItemConv.h"
#include "game/Storage.h"
#include "mod/Commands.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace abp::admin {

// Command parameter types need external linkage.
enum class AbpAdminPlayerName {};
enum class AbpAdminBackpackUuid {};

struct AbpAdminListParams {
    ll::command::SoftEnum<AbpAdminPlayerName> playerName;
};
struct AbpAdminGiveParams {
    CommandSelector<Player>                targets;
    ll::command::SoftEnum<AbpAdminBackpackUuid> backpackUuid;
};
struct AbpAdminRemoveNonPlayerParams {
    bool onlyWithEmptyInventory = false;
};

namespace {

Player* playerOf(CommandOrigin const& origin) {
    auto* e = origin.getEntity();
    return (e && e->isPlayer()) ? static_cast<Player*>(e) : nullptr;
}

template <class... Args>
std::string say(CommandOrigin const& origin, std::string_view key, std::string_view fallback, Args const&... args) {
    if (auto* p = playerOf(origin)) return trFor(*p, key, fallback, args...);
    return tr(key, fallback, args...);
}

/// `/backpack` is open to everybody (per-overload checks); `/backpackadmin` is registered for operators.
bool isOperator(CommandOrigin const& origin, CommandOutput& output) {
    if (origin.getPermissionsLevel() >= CommandPermissionLevel::GameDirectors) return true;
    output.error(say(origin, "abp.admin.err.permission", "You need operator permission."));
    return false;
}

std::string formatAccessTime(std::int64_t millis) {
    std::time_t t = static_cast<std::time_t>(millis / 1000);
    std::tm     tm{};
    localtime_s(&tm, &t);
    return core::shortDateTime(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
}

/// One `/backpackadmin list` line: "§a<name>§r, <Cloth> <Trim>, <player>, <date>" with the colour words in
/// the nearest text colour of the backpack's colours.
std::string listLine(CommandOrigin const& origin, AccessLogRecord const& r) {
    auto cloth = say(origin, "abp.admin.backpackadmin.list.cloth_color", "Cloth");
    auto trim  = say(origin, "abp.admin.backpackadmin.list.trim_color", "Trim");
    std::string line = "§a" + r.backpackName + "§r, ";
    line += std::string("§") + core::nearestFormatCode(r.clothColor & 0xFFFFFF) + cloth + "§r ";
    line += std::string("§") + core::nearestFormatCode(r.trimColor & 0xFFFFFF) + trim + "§r, ";
    line += r.playerName + ", " + formatAccessTime(r.accessTime);
    return line;
}

void printBackpackList(CommandOrigin const& origin, CommandOutput& output, std::vector<AccessLogRecord> logs) {
    std::stable_sort(logs.begin(), logs.end(), [](auto const& a, auto const& b) { return a.accessTime > b.accessTime; });
    output.success(say(origin, "abp.admin.backpackadmin.list.header", "Backpack Name, Colors, Last Open By, Last Open On"));
    for (auto const& r : logs) output.success(listLine(origin, r));
    log().info("backpackadmin: list -> {} backpacks", logs.size());
}

void list(CommandOrigin const& origin, CommandOutput& output, std::optional<std::string> const& playerName) {
    auto& accessLog = AccessLog::get();
    printBackpackList(origin, output, playerName ? accessLog.logsForPlayer(*playerName) : accessLog.all());
}

void giveBackpackToPlayer(ItemStack const& backpack, Player& p) {
    ItemStack copy = backpack;
    if (!p.addAndRefresh(copy)) p.drop(copy, false);
}

void give(CommandOrigin const& origin, CommandOutput& output, CommandSelector<Player> const& targets, std::string const& uuid) {
    auto record = AccessLog::get().find(uuid);
    if (!record) {
        log().info("backpackadmin: give {} -> no access log record", uuid);
        return; // GiveCommand: nothing happens for an unknown uuid
    }
    auto tier = game::backpackTierOf(record->backpackItemRegistryName);
    if (!tier) return;
    auto backpack = game::makeBackpackItem(*tier, uuid);
    if (backpack.isNull()) return output.error("Backpack items are not registered (behavior pack missing?).");
    if (record->backpackName != backpackDisplayName(*tier)) {
        backpack.setCustomName(Bedrock::Safety::RedactableString{record->backpackName});
    }
    // GiveCommand writes the logged colours into the backpack it gives (the columns taken follow the
    // storage's installed upgrades here).
    if (auto bp = game::Storage::get().find(uuid)) {
        bp->data().mainColor   = record->clothColor;
        bp->data().accentColor = record->trimColor;
        bp->markDirty();
    }
    std::vector<Player*> players;
    for (auto* p : targets.results(origin)) {
        if (p) players.push_back(p);
    }
    for (auto* p : players) giveBackpackToPlayer(backpack, *p);
    if (players.size() == 1) {
        output.success(say(origin, "abp.admin.backpackadmin.give.success", "Gave backpack to {}", players.front()->getRealName()));
    } else {
        output.success(say(origin, "abp.admin.backpackadmin.give.success", "Gave backpack to {}", players.size()));
    }
    log().info("backpackadmin: gave backpack {} to {} players", uuid, players.size());
}

void removeNonPlayerCommand(CommandOrigin const& origin, CommandOutput& output, bool onlyWithEmptyInventory) {
    auto removed = removeNonPlayer(onlyWithEmptyInventory);
    output.success(say(origin, "abp.admin.backpackadmin.remove_non_player.success", "Removed {} non player backpacks", removed.size()));
    log().info("backpackadmin: removeNonPlayer {} -> {}", onlyWithEmptyInventory, removed.size());
}

/// The built-in overloads. `gate` true: check operator permission per overload (`/backpack`).
void addOverloads(ll::command::CommandHandle& cmd, bool gate) {
    cmd.overload().text("list").execute([gate](CommandOrigin const& origin, CommandOutput& output) {
        if (gate && !isOperator(origin, output)) return;
        list(origin, output, std::nullopt);
    });
    cmd.overload<AbpAdminListParams>().text("list").required("playerName").execute(
        [gate](CommandOrigin const& origin, CommandOutput& output, AbpAdminListParams const& params) {
            if (gate && !isOperator(origin, output)) return;
            list(origin, output, std::string(params.playerName));
        }
    );
    cmd.overload<AbpAdminGiveParams>().text("give").required("targets").required("backpackUuid").execute(
        [gate](CommandOrigin const& origin, CommandOutput& output, AbpAdminGiveParams const& params) {
            if (gate && !isOperator(origin, output)) return;
            give(origin, output, params.targets, std::string(params.backpackUuid));
        }
    );
    // Bedrock matches literals lower-cased: the `removeNonPlayer` is typed as is and matches.
    cmd.overload<AbpAdminRemoveNonPlayerParams>().text("removenonplayer").required("onlyWithEmptyInventory").execute(
        [gate](CommandOrigin const& origin, CommandOutput& output, AbpAdminRemoveNonPlayerParams const& params) {
            if (gate && !isOperator(origin, output)) return;
            removeNonPlayerCommand(origin, output, params.onlyWithEmptyInventory);
        }
    );
}

std::uint64_t gSuggestionRevision = ~0ull;

void registerAbpAdmin() {
    if (!ll::service::getCommandRegistry()) return;
    auto& registrar = ll::command::CommandRegistrar::getServerInstance();
    auto& cmd       = registrar.getOrCreateCommand(
        "backpackadmin",
        "Backpack admin commands",
        CommandPermissionLevel::GameDirectors
    );
    addOverloads(cmd, false);
    for (auto const& fn : AbpAdminSubcommandRegistry::entries()) fn(cmd);
    gSuggestionRevision = ~0ull;
    refreshAbpAdminSuggestions();
}

} // namespace

void refreshAbpAdminSuggestions() {
    if (!ll::service::getCommandRegistry()) return;
    auto& accessLog = AccessLog::get();
    if (gSuggestionRevision == accessLog.revision()) return;
    gSuggestionRevision = accessLog.revision();
    auto& registrar     = ll::command::CommandRegistrar::getServerInstance();
    // Suggestions follow the log at once (the 30 s cache only applies to `list <playerName>`).
    std::set<std::string> names;
    for (auto const& r : accessLog.all()) names.insert(r.playerName);
    std::vector<std::string> nameList(names.begin(), names.end());
    if (registrar.hasSoftEnum(ll::command::enum_name_v<ll::command::SoftEnum<AbpAdminPlayerName>>)) {
        registrar.setSoftEnumValues(ll::command::enum_name_v<ll::command::SoftEnum<AbpAdminPlayerName>>, nameList);
    }
    if (registrar.hasSoftEnum(ll::command::enum_name_v<ll::command::SoftEnum<AbpAdminBackpackUuid>>)) {
        registrar.setSoftEnumValues(ll::command::enum_name_v<ll::command::SoftEnum<AbpAdminBackpackUuid>>, accessLog.uuids());
    }
}

namespace {

class AbpAdminCommandModule {
public:
    void install() {
        registerAbpAdmin(); // BDS: the registry exists when mods are enabled
        auto& bus = ll::event::EventBus::getInstance();
        mListeners.push_back(bus.emplaceListener<ll::event::command::ServerCommandRegisterEvent>([](auto&) {
            registerAbpAdmin(); // client: every local world
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([this](auto&) {
            if (++mTicks % 20 == 0) refreshAbpAdminSuggestions();
        }));
    }
    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
    }

private:
    std::vector<ll::event::ListenerPtr> mListeners;
    int                                 mTicks = 0;
};

} // namespace
} // namespace abp::admin

ABP_REGISTER_MODULE_ORDERED(abp::admin::AbpAdminCommandModule, abp::ModuleOrder::Commands + 1);
// `/backpack list|give|removeNonPlayer` keep working (operators only).
ABP_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) { abp::admin::addOverloads(cmd, true); });
