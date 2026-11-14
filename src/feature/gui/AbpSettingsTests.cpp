// Selftests of the backpack settings categories and the settings screen actions (P2-2).

#include <filesystem>

#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"

#include "core/Appearance.h"
#include "feature/gui/Gui.h"
#include "feature/gui/AbpSettings.h"
#include "feature/templates/Templates.h"
#include "game/Backpack.h"
#include "game/ItemConv.h"
#include "selftest/SelfTest.h"

namespace abp::feature::gui {
namespace {
using selftest::Context;
using core::Tier;
using nlohmann::json;

core::ItemStackData item(std::string_view id, int count) { return game::ItemConv::toData(game::ItemConv::make(id, count)); }

json act(std::string what, json extra = json::object()) {
    extra["what"] = std::move(what);
    return extra;
}

/// Restores the player's main settings / search phrase / templates after a test.
struct PlayerGuard {
    Player& p;
    ~PlayerGuard() {
        for (auto const& m : mainSettings()) setPlayerOption(p, m.key, std::nullopt);
        setPlayerOption(p, "searchPhrase", std::nullopt);
        templates::clearPlayerTemplates(p);
    }
};

ABP_REGISTER_SELFTEST("storagesettings.main_resolution", [](Context& ctx) {
    auto& p = ctx.player();
    PlayerGuard guard{p};
    auto        bp = ctx.createBackpack(Tier::Leather);
    for (auto const& m : mainSettings()) setPlayerOption(p, m.key, std::nullopt);
    // Defaults (SettingsManager / BackpackMainSettingsCategory).
    ctx.check(settingValue(p, *bp, MainSetting::ShiftClickOpenTab), "shiftClickOpenTab defaults to true");
    ctx.check(settingValue(p, *bp, MainSetting::KeepTabOpen), "keepTabOpen defaults to true");
    ctx.check(settingValue(p, *bp, MainSetting::KeepSearchPhrase), "keepSearchPhrase defaults to true");
    ctx.check(settingValue(p, *bp, MainSetting::AnotherPlayerCanOpen), "anotherPlayerCanOpen defaults to true");

    settingsScreenOpened(p, *bp);
    auto j = settingsScreenJson(p, *bp);
    ctx.check(j.value("context", "") == "player", "context starts at Player");
    // Player context: player level value; a value equal to the default is removed.
    ctx.check(handleSettingsScreenAction(p, *bp, act("main", {{"name", "keepTabOpen"}})), "toggle keepTabOpen (player)");
    ctx.check(playerSetting(p, MainSetting::KeepTabOpen) == std::optional<bool>(false), "player value stored");
    ctx.check(!settingValue(p, *bp, MainSetting::KeepTabOpen), "backpack inherits the player value");
    ctx.check(handleSettingsScreenAction(p, *bp, act("main", {{"name", "keepTabOpen"}})), "toggle back");
    ctx.check(!playerSetting(p, MainSetting::KeepTabOpen).has_value(), "default value removed from the player");

    // Storage context.
    ctx.check(handleSettingsScreenAction(p, *bp, act("context")), "switch to Backpack context");
    ctx.check(settingsScreenJson(p, *bp).value("context", "") == "storage", "context Backpack");
    ctx.check(handleSettingsScreenAction(p, *bp, act("main", {{"name", "shiftClickOpenTab"}})), "toggle shiftClickOpenTab (backpack)");
    ctx.check(storageSetting(*bp, MainSetting::ShiftClickOpenTab) == std::optional<bool>(false), "stored on the backpack");
    ctx.check(!effective(&p, *bp).openTabFirst, "the phase-1 option follows the main setting");
    ctx.check(playerSettingOrDefault(p, MainSetting::ShiftClickOpenTab), "player value untouched");
    // A storage value equal to the player's value is removed.
    setPlayerSetting(p, MainSetting::ShiftClickOpenTab, false);
    ctx.check(handleSettingsScreenAction(p, *bp, act("main", {{"name", "shiftClickOpenTab"}})), "toggle on on the backpack");
    ctx.check(storageSetting(*bp, MainSetting::ShiftClickOpenTab) == std::optional<bool>(true), "true differs from player false");
    ctx.check(handleSettingsScreenAction(p, *bp, act("main", {{"name", "shiftClickOpenTab"}})), "toggle off again");
    ctx.check(!storageSetting(*bp, MainSetting::ShiftClickOpenTab).has_value(), "equal to the player value: removed");

    // Reopening the settings screen resets the context to Player.
    settingsScreenOpened(p, *bp);
    ctx.check(settingsScreenJson(p, *bp).value("context", "") == "player", "context reset on open");

    // Search phrase: kept at player level while the player keeps search phrases.
    setSearchPhrase(p, *bp, "dia");
    ctx.check(searchPhrase(p, *bp) == "dia", "search phrase kept");
    ctx.check(handleSettingsScreenAction(p, *bp, act("main", {{"name", "keepSearchPhrase"}})), "keep search phrase off");
    ctx.check(searchPhrase(p, *bp).empty(), "phrase cleared when keeping is turned off");
});

ABP_REGISTER_SELFTEST("storagesettings.memory_nbt", [](Context& ctx) {
    auto& p  = ctx.player();
    auto  bp = ctx.createBackpack(Tier::Leather);
    auto& inv = bp->inventory();
    auto  named = game::ItemConv::make("minecraft:diamond_sword", 1);
    game::ItemConv::setDisplay(named, "Excalibur");
    inv.set(0, game::ItemConv::toData(named));
    inv.set(1, item("minecraft:dirt", 5));
    ctx.check(memoryIgnoresNbt(*bp), "Ignore NBT is the default");
    ctx.check(!handleSettingsScreenAction(p, *bp, act("memory", {{"op", "select"}, {"index", 2}})), "an empty slot can't be selected");
    ctx.check(handleSettingsScreenAction(p, *bp, act("memory", {{"op", "select"}, {"index", 0}})), "select the sword slot");
    ctx.check(memory(*bp)[0].id == "minecraft:diamond_sword" && memory(*bp)[0].nbt().empty(), "ignore NBT remembers the item only");
    inv.set(0, {});
    auto plain = item("minecraft:diamond_sword", 1);
    ctx.check(inv.insertInto(0, plain, {core::Source::Player, nullptr}, true).inserted == 1, "a plain sword fits (NBT ignored)");

    // Match NBT: the slot remembers the full stack again (setIgnoreNbt re-selects the slots).
    inv.set(0, game::ItemConv::toData(named));
    ctx.check(handleSettingsScreenAction(p, *bp, act("memoryIgnoreNbt", {{"value", false}})), "Match NBT");
    ctx.check(!memoryIgnoresNbt(*bp), "mode stored");
    ctx.check(!memory(*bp)[0].nbt().empty(), "the named sword was remembered with its NBT");
    inv.set(0, {});
    ctx.check(inv.insertInto(0, plain, {core::Source::Player, nullptr}, true).inserted == 0, "a plain sword is refused (NBT matched)");
    ctx.check(inv.insertInto(0, game::ItemConv::toData(named), {core::Source::Player, nullptr}, true).inserted == 1, "the named one fits");

    ctx.check(handleSettingsScreenAction(p, *bp, act("memory", {{"op", "selectAll"}})), "select all");
    ctx.check(memory(*bp)[1].id == "minecraft:dirt", "every slot with an item is remembered");
    ctx.check(handleSettingsScreenAction(p, *bp, act("memory", {{"op", "unselect"}, {"index", 1}})), "unselect");
    ctx.check(memory(*bp)[1].empty(), "forgotten");
    ctx.check(handleSettingsScreenAction(p, *bp, act("memory", {{"op", "unselectAll"}})), "unselect all");
    ctx.check(memoryCount(*bp) == 0, "all forgotten");
});

ABP_REGISTER_SELFTEST("storagesettings.no_sort", [](Context& ctx) {
    auto& p  = ctx.player();
    auto  bp = ctx.createBackpack(Tier::Leather);
    ctx.check(noSortColor(*bp) == static_cast<int>(core::DyeColor::Lime), "lime by default");
    ctx.check(handleSettingsScreenAction(p, *bp, act("noSortColor", {{"dir", 1}})), "next colour");
    ctx.check(noSortColor(*bp) == static_cast<int>(core::DyeColor::Pink), "lime -> pink");
    ctx.check(handleSettingsScreenAction(p, *bp, act("noSortColor", {{"dir", -1}})), "previous colour");
    ctx.check(handleSettingsScreenAction(p, *bp, act("noSortColor", {{"dir", -1}})), "previous colour again");
    ctx.check(noSortColor(*bp) == static_cast<int>(core::DyeColor::Yellow), "lime -> yellow going back");
    setNoSortColor(*bp, 15);
    ctx.check(handleSettingsScreenAction(p, *bp, act("noSortColor", {{"dir", 1}})) && noSortColor(*bp) == 0, "black wraps to white");
    ctx.check(handleSettingsScreenAction(p, *bp, act("noSort", {{"op", "select"}, {"index", 3}})), "select slot 3");
    ctx.check(!handleSettingsScreenAction(p, *bp, act("noSort", {{"op", "select"}, {"index", 3}})), "already selected");
    ctx.check(noSort(*bp)[3], "slot 3 not sorted");
    ctx.check(handleSettingsScreenAction(p, *bp, act("noSort", {{"op", "selectAll"}})) && noSortCount(*bp) == bp->inventory().size(), "select all");
    ctx.check(handleSettingsScreenAction(p, *bp, act("noSort", {{"op", "unselectAll"}})) && noSortCount(*bp) == 0, "unselect all");
});

ABP_REGISTER_SELFTEST("storagesettings.item_display", [](Context& ctx) {
    auto& p  = ctx.player();
    auto  bp = ctx.createBackpack(Tier::Leather);
    bp->inventory().set(4, item("minecraft:diamond", 3));
    auto d = itemDisplay(*bp);
    ctx.check(d.slots.empty() && d.color == static_cast<int>(core::DyeColor::Red), "nothing selected, red");
    ctx.check(handleSettingsScreenAction(p, *bp, act("itemDisplay", {{"op", "select"}, {"index", 4}})), "select slot 4");
    ctx.check(!handleSettingsScreenAction(p, *bp, act("itemDisplay", {{"op", "select"}, {"index", 5}})), "limit 1: another slot can't be selected");
    auto shown = displayedItem(*bp);
    ctx.require(shown.has_value(), "displayed item");
    ctx.check(shown->slot == 4 && shown->item.id == "minecraft:diamond" && shown->item.count == 1, "the slot's item");
    for (int i = 0; i < 3; ++i) handleSettingsScreenAction(p, *bp, act("itemDisplay", {{"op", "rotate"}, {"index", 4}, {"clockwise", true}}));
    ctx.check(itemDisplay(*bp).rotations.at(0) == 135, "3 x 45 clockwise");
    handleSettingsScreenAction(p, *bp, act("itemDisplay", {{"op", "rotate"}, {"index", 4}, {"clockwise", false}}));
    ctx.check(itemDisplay(*bp).rotations.at(0) == 90, "counter-clockwise");
    ctx.check(!handleSettingsScreenAction(p, *bp, act("itemDisplay", {{"op", "zOffset"}, {"index", 4}, {"change", 1}})), "no depth setting");
    ctx.check(handleSettingsScreenAction(p, *bp, act("itemDisplay", {{"op", "color"}, {"dye", 11}})) && itemDisplay(*bp).color == 11, "blue");
    // An empty display slot shows its memory item.
    setMemory(*bp, 4, item("minecraft:diamond", 1));
    bp->inventory().set(4, {});
    shown = displayedItem(*bp);
    ctx.check(shown && shown->item.id == "minecraft:diamond", "memory item displayed");
    ctx.check(handleSettingsScreenAction(p, *bp, act("itemDisplay", {{"op", "unselect"}, {"index", 4}})), "unselect");
    ctx.check(itemDisplay(*bp).slots.empty() && !displayedItem(*bp), "nothing displayed");
    ctx.check(handleSettingsScreenAction(p, *bp, act("itemDisplay", {{"op", "select"}, {"index", 5}})), "another slot now");
});

ABP_REGISTER_SELFTEST("storagesettings.templates", [](Context& ctx) {
    auto& p = ctx.player();
    PlayerGuard guard{p};
    templates::clearPlayerTemplates(p);
    auto src = ctx.createBackpack(Tier::Iron);
    src->inventory().set(0, item("minecraft:dirt", 2));
    setMemory(*src, 0, item("minecraft:dirt", 1));
    setNoSort(*src, 50, true); // beyond a leather backpack's 27 slots
    setNoSortColor(*src, 3);
    selectItemDisplaySlot(*src, 0);
    rotateItemDisplay(*src, 0, true);

    settingsScreenOpened(p, *src);
    auto t = settingsScreenJson(p, *src)["templates"];
    ctx.check(t["save"].value("name", "") == "1", "first save slot is ordinal 1");
    ctx.check(t["load"].value("index", 0) == -1 || t["load"].value("source", "x") != "", "no own saves yet");
    ctx.check(handleSettingsScreenAction(p, *src, act("template", {{"op", "save"}})), "save to slot 1");
    ctx.check(templates::playerTemplates(p).count(1) == 1, "stored as ordinal 1");
    t = settingsScreenJson(p, *src)["templates"];
    ctx.check(t["save"].value("name", "") == "1" && t["load"].value("name", "") == "1", "slot 1 selected for save and load");
    ctx.check(handleSettingsScreenAction(p, *src, act("template", {{"op", "scrollSave"}, {"next", true}})), "scroll save");
    ctx.check(settingsScreenJson(p, *src)["templates"]["save"].value("name", "") == "2", "next ordinal slot 2");
    ctx.check(handleSettingsScreenAction(p, *src, act("template", {{"op", "scrollSave"}, {"next", true}})), "scroll save to the name slot");
    ctx.check(settingsScreenJson(p, *src)["templates"]["save"].value("custom", false), "edit-name slot");
    ctx.check(handleSettingsScreenAction(p, *src, act("template", {{"op", "save"}, {"name", "Ores"}})), "save named");
    ctx.check(templates::playerNamedTemplates(p).count("Ores") == 1, "named template stored");

    auto dst = ctx.createBackpack(Tier::Leather);
    settingsScreenOpened(p, *dst);
    t = settingsScreenJson(p, *dst)["templates"];
    ctx.check(t["load"].value("name", "") == "1", "load slot 1");
    ctx.check(t["load"].value("tooLarge", false), "\"Settings don't fit in this storage\" (no-sort slot 50)");
    ctx.check(t["load"]["preview"]["memory"].contains("0"), "preview has the template's memory");
    ctx.check(handleSettingsScreenAction(p, *dst, act("template", {{"op", "load"}})), "load");
    ctx.check(memory(*dst)[0].id == "minecraft:dirt", "memory loaded");
    ctx.check(noSortColor(*dst) == 3 && noSortCount(*dst) == 0, "no-sort colour loaded, out-of-range slot dropped");
    ctx.check(itemDisplay(*dst).slots == std::vector<int>{0} && itemDisplay(*dst).rotations.at(0) == 45, "item display loaded");

    // Export: a pack template, listed with its source.
    ctx.check(!handleSettingsScreenAction(p, *dst, act("template", {{"op", "export"}, {"fileName", ""}})), "empty name refused");
    ctx.check(!handleSettingsScreenAction(p, *dst, act("template", {{"op", "export"}, {"fileName", "bad*name"}})), "invalid characters refused");
    ctx.check(handleSettingsScreenAction(p, *dst, act("template", {{"op", "export"}, {"fileName", "My Ores"}})), "export");
    bool found = false;
    for (auto const& [pack, list] : templates::packSettingsTemplates()) {
        if (pack.ends_with("_abp_templates") && list.count("My Ores")) found = true;
    }
    ctx.check(found, "exported template registered as 'My Ores'");
    // Clean the exported pack folder.
    std::error_code ec;
    for (auto const& e : std::filesystem::directory_iterator(templates::templatePacksRoot(), ec)) {
        if (e.path().filename().string().ends_with("_abp_templates")) std::filesystem::remove_all(e.path(), ec);
    }
    templates::reloadTemplatePacks();
});

// BackpackWrapper.sort -> InventorySorter.sortHandler(.., noSort + memory slots):
// excluded slots keep their stacks and are only topped up with the very same item; an empty memory
// slot stays empty.
ABP_REGISTER_SELFTEST("storagesettings.sort_excluded_slots", [](Context& ctx) {
    auto& p  = ctx.player();
    auto  bp = ctx.createBackpack(Tier::Leather);
    auto& inv = bp->inventory();
    auto named = game::ItemConv::make("minecraft:diamond_sword", 1);
    game::ItemConv::setDisplay(named, "Excalibur");
    auto namedData = game::ItemConv::toData(named);
    auto plain     = item("minecraft:diamond_sword", 1);
    inv.set(5, namedData);
    ctx.check(handleSettingsScreenAction(p, *bp, act("memoryIgnoreNbt", {{"value", false}})), "Match NBT");
    ctx.check(handleSettingsScreenAction(p, *bp, act("memory", {{"op", "select"}, {"index", 5}})), "remember the named sword in slot 5");
    inv.set(5, {});
    inv.set(9, plain);
    inv.set(12, namedData);
    ctx.check(sortBackpack(*bp, core::SortBy::Name), "sorted");
    ctx.check(inv.at(5).empty(), "the empty memory slot is left empty: " + inv.at(5).id);
    ctx.check(inv.at(0).id == "minecraft:diamond_sword" && inv.at(1).id == "minecraft:diamond_sword", "both swords laid out from slot 0");

    // A no-sort slot holding dirt is topped up with plain dirt only; renamed dirt stays apart.
    auto fancy = game::ItemConv::make("minecraft:dirt", 3);
    game::ItemConv::setDisplay(fancy, "Fancy dirt");
    auto fancyData = game::ItemConv::toData(fancy);
    setNoSort(*bp, 7, true);
    inv.set(7, item("minecraft:dirt", 5));
    inv.set(20, item("minecraft:dirt", 10));
    inv.set(21, fancyData);
    ctx.check(sortBackpack(*bp, core::SortBy::Name), "sorted again");
    ctx.check(inv.at(7).sameItem(item("minecraft:dirt", 1)) && inv.at(7).count == 15, "no-sort slot topped up: " + std::to_string(inv.at(7).count));
    int fancyCount = 0;
    for (int i = 0; i < inv.size(); ++i) {
        if (inv.at(i).sameItem(fancyData)) fancyCount += inv.at(i).count;
    }
    ctx.check(fancyCount == 3, "renamed dirt is not merged into the no-sort slot");
    ctx.check(inv.at(5).empty(), "the memory slot is still empty");
});

} // namespace
} // namespace abp::feature::gui
