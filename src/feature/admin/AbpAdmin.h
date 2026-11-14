#pragma once

// `/backpackadmin`: the admin command,
// operators only (op level 2). Built-in: `list [playerName]`,
// `give <targets> <backpackUuid>`, `removeNonPlayer <onlyWithEmptyInventory>`. Other units may add
// sub-commands with `ABP_REGISTER_ADMIN_SUBCOMMAND`; the built-in overloads are also reachable as
// `/backpack ...`.

#include <functional>

#include "mod/Registry.h"

namespace ll::command {
class CommandHandle;
}

namespace abp::admin {

/// Adds overloads to `/backpackadmin`. Runs every time the command is (re)registered (BDS start, every client
/// local world). Gate nothing yourself: the root already requires operator permission.
using AbpAdminSubcommandFn       = std::function<void(ll::command::CommandHandle&)>;
using AbpAdminSubcommandRegistry = StaticRegistry<AbpAdminSubcommandFn>;

namespace detail {
struct AbpAdminSubcommandRegistrar {
    explicit AbpAdminSubcommandRegistrar(AbpAdminSubcommandFn fn) { AbpAdminSubcommandRegistry::add(std::move(fn)); }
};
} // namespace detail

/// Refreshes the `/backpackadmin` argument suggestions (player names and backpack uuids from the access log).
void refreshAbpAdminSuggestions();

} // namespace abp::admin

#define ABP_REGISTER_ADMIN_SUBCOMMAND(...)                                                                               \
    static ::abp::admin::detail::AbpAdminSubcommandRegistrar ABP_UNIQUE(abp_admin_subcommand_)(__VA_ARGS__)
