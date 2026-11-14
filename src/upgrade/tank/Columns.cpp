#include "upgrade/tank/Columns.h"

#include <algorithm>
#include <map>

#include "core/Capacity.h"
#include "core/ColumnsFit.h"
#include "game/Backpack.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "upgrade/UpgradeManager.h"

namespace abp::upgrade::columns {

namespace {
std::map<std::string, int, std::less<>>& registry() {
    static std::map<std::string, int, std::less<>> r;
    return r;
}

int baseSlots(game::Backpack const& bp) { return bp.data().baseStorageSlots(); }

core::columns::FitResult fitFor(game::Backpack& bp, int targetColumns) {
    auto&             inv = bp.inventory();
    std::vector<bool> occupied(static_cast<std::size_t>(inv.size()), false);
    for (int i = 0; i < inv.size(); ++i) occupied[static_cast<std::size_t>(i)] = !inv.at(i).empty();
    return core::columns::fit(occupied, core::capacity::slotsAfterColumns(baseSlots(bp), targetColumns));
}
} // namespace

void registerColumns(std::string upgradeId, int columns) { registry()[std::move(upgradeId)] = std::max(0, columns); }

int columnsOf(core::ItemStackData const& upgradeItem) {
    auto* type = upgradeItem.empty() ? nullptr : upgradeFor(upgradeItem);
    if (!type) return 0;
    auto it = registry().find(type->info().id);
    return it == registry().end() ? 0 : it->second;
}

int expected(game::Backpack const& backpack, int excludeSlot) {
    int         n     = 0;
    auto const& slots = backpack.data().upgrades;
    for (int i = 0; i < static_cast<int>(slots.size()); ++i) {
        if (i != excludeSlot) n += columnsOf(slots[static_cast<std::size_t>(i)].item);
    }
    return n;
}

Check check(game::Backpack& backpack, int targetColumns) {
    Check c;
    int   current = backpack.data().columnsTaken;
    if (targetColumns <= current) return c;
    c.slotsNeeded = (targetColumns - current) * core::capacity::slotRows(baseSlots(backpack));
    auto fit      = fitFor(backpack, targetColumns);
    c.ok          = fit.fits;
    c.errorSlots  = std::move(fit.errorSlots);
    return c;
}

bool canInstall(UpgradeContext const& ctx, core::ItemStackData const& item, std::string& reason) {
    int add = columnsOf(item);
    if (add <= 0) return true;
    auto& bp = ctx.backpack();
    // The slot being installed into is empty (a swap removed the old upgrade first).
    auto c = check(bp, expected(bp, ctx.slot()) + add);
    if (c.ok) return true;
    auto* type = upgradeFor(item);
    auto  name = type ? displayName(*type, ctx.player()) : item.id;
    reason     = ctx.player() ? trFor(
                                *ctx.player(),
                                "abp.columns.needs_occupied_slots",
                                "There's an item in at least one of the last {} slots needed for {}",
                                c.slotsNeeded,
                                name
                            )
                              : tr("abp.columns.needs_occupied_slots", "There's an item in at least one of the last {} slots needed for {}", c.slotsNeeded, name);
    return false;
}

bool apply(game::Backpack& backpack, int targetColumns) {
    targetColumns = std::max(0, targetColumns);
    auto& data    = backpack.data();
    if (targetColumns == data.columnsTaken) return true;
    int  base   = baseSlots(backpack);
    int  target = core::capacity::slotsAfterColumns(base, targetColumns);
    auto& inv   = backpack.inventory();
    if (target >= inv.size()) {
        // Giving columns back: the inventory grows, items stay where they are.
        data.columnsTaken = targetColumns;
        inv.resize(target);
        backpack.markDirty();
        return true;
    }
    // Taking columns: the slots that go away must be empty; nothing is moved.
    if (!fitFor(backpack, targetColumns).fits) return false;
    auto lost = inv.resize(target);
    for (auto const& d : lost) {
        if (!d.empty()) log().error("inventory columns: {} x {} dropped while shrinking backpack {}", d.count, d.id, backpack.uuid());
    }
    data.columnsTaken = targetColumns;
    backpack.markDirty();
    return true;
}

bool sync(game::Backpack& backpack, int excludeSlot) {
    int want = expected(backpack, excludeSlot);
    if (want == backpack.data().columnsTaken) return true;
    return apply(backpack, want);
}

} // namespace abp::upgrade::columns
