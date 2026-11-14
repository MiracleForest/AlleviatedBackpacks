#pragma once

// P2-2 - backpack settings with the behaviour of `BackpackSettingsHandler` and the
// The settings categories, plus the server side of the native settings screen.
//
// Categories, stored in the backpack's `settings.gui` section (see Gui.h for memory / no-sort):
//   main ("backpackGlobal")  shiftClickOpenTab, keepTabOpen, keepSearchPhrase, anotherPlayerCanOpen,
//                            searchPhrase. Storage value -> player value -> default (
//                            SettingsManager); a storage value equal to the player's is removed.
//   item display             one selectable slot (the limit 1; unselectSlot), rotation
//                            per slot (0..315, 45 degree steps), colour (default red).
//   no sort                  slots (Gui.h) + colour (default lime).
//   memory                   remembered items (Gui.h) + Match / Ignore NBT (default ignore).
//
// Settings templates use the settings NBT layout as JSON:
//   {"backpackGlobal":{..}, "item_display":{"slots":[..],"rotations":{"slot":deg},"color":n},
//    "no_sort":{"selectedSlots":[..],"color":n},
//    "memory":{"slotFilterItems":{"slot":"id"},"slotFilterStacks":{"slot":{item}},"ignoreNbt":b}}

#include <optional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "core/ItemStackData.h"

class Player;

namespace abp::game {
class Backpack;
}

namespace abp::feature::gui {

// ---- main settings (SettingsManager / BackpackMainSettingsCategory) -------------
enum class MainSetting { ShiftClickOpenTab, KeepTabOpen, KeepSearchPhrase, AnotherPlayerCanOpen };

struct MainSettingInfo {
    MainSetting setting;
    char const* key; ///< tag name in the settings NBT
    bool        defaultValue;
};
[[nodiscard]] MainSettingInfo const&              mainSettingInfo(MainSetting s);
[[nodiscard]] std::vector<MainSettingInfo> const& mainSettings();
[[nodiscard]] std::optional<MainSetting>          mainSettingByKey(std::string_view key);

/// Player level value (persisted per player), if set.
[[nodiscard]] std::optional<bool> playerSetting(Player& player, MainSetting s);
[[nodiscard]] bool                playerSettingOrDefault(Player& player, MainSetting s);
/// setPlayerSetting: stores the value, or removes it when it equals the default.
void setPlayerSetting(Player& player, MainSetting s, bool value);
/// The backpack's own value, if set.
[[nodiscard]] std::optional<bool> storageSetting(game::Backpack const& backpack, MainSetting s);
/// getSettingValue: storage value -> player value -> default.
[[nodiscard]] bool settingValue(Player& player, game::Backpack const& backpack, MainSetting s);
/// setSetting: stores the value on the backpack, or removes it when it equals the player's.
void setStorageSetting(Player& player, game::Backpack& backpack, MainSetting s, bool value);

/// StorageContainerMenuBase.getSearchPhrase / setSearchPhrase.
[[nodiscard]] std::string searchPhrase(Player& player, game::Backpack const& backpack);
void                      setSearchPhrase(Player& player, game::Backpack& backpack, std::string const& phrase);

/// AnotherPlayerBackpackOpenPayload: `wearer`'s backpack may be opened by another player
/// (server config `allowOpeningOtherPlayerBackpacks` and the backpack's / wearer's setting).
[[nodiscard]] bool anotherPlayerCanOpen(Player& wearer, game::Backpack const& backpack);

// ---- no sort colour ----------------------------------------------------------------------------------
/// Dye colour id (0..15, `core::DyeColor`) of the no-sort overlay; default lime.
[[nodiscard]] int  noSortColor(game::Backpack const& backpack);
void               setNoSortColor(game::Backpack& backpack, int dye);

// ---- item display (ItemDisplaySettingsCategory, one slot) ------------------------
inline constexpr int kItemDisplayLimit = 1;

struct ItemDisplay {
    std::vector<int> slots;    ///< selected slots in selection order (at most kItemDisplayLimit)
    std::vector<int> rotations; ///< per entry of `slots`: 0, 45, ... 315
    int              color = 14; ///< dye id, default red
};
[[nodiscard]] ItemDisplay itemDisplay(game::Backpack const& backpack);
/// Returns whether the selection changed (nothing happens when the limit is reached).
bool selectItemDisplaySlot(game::Backpack& backpack, int slot);
bool unselectItemDisplaySlot(game::Backpack& backpack, int slot);
void rotateItemDisplay(game::Backpack& backpack, int slot, bool clockwise);
void setItemDisplayColor(game::Backpack& backpack, int dye);

/// What the placed-backpack / worn renderers draw (P2-11): the displayed item of the first selected
/// slot (its stack, else its memory item), rotation and colour. nullopt = nothing displayed.
struct DisplayedItem {
    int                 slot = -1;
    core::ItemStackData item; ///< count 1
    int                 rotation = 0;
    int                 color    = 14;
};
[[nodiscard]] std::optional<DisplayedItem> displayedItem(game::Backpack& backpack);

// ---- templates ------------------------------------------------------------------------------------
/// The backpack's settings in the settings NBT layout (see the top of this file).
[[nodiscard]] nlohmann::json settingsTemplateOf(game::Backpack& backpack);
/// loadTemplate: every category's `overwriteWith` (main settings are kept).
void loadSettingsTemplate(game::Backpack& backpack, nlohmann::json const& tmpl);
/// isLargerThanNumberOfSlots for any category.
[[nodiscard]] bool settingsTemplateTooLarge(nlohmann::json const& tmpl, int slots);

/// A template's view for the "load hovered" preview of the settings screen: memory items, no-sort
/// slots and colour, item display slots / colour / rotations.
[[nodiscard]] nlohmann::json settingsTemplatePreview(nlohmann::json const& tmpl);

// ---- native settings screen ------------------------------------------------------------------------
/// `m.extra["settings"]` of the native backpack screen for `player`.
[[nodiscard]] nlohmann::json settingsScreenJson(Player& player, game::Backpack& backpack);
/// The player opened the settings screen (the mod creates a new settings container: context PLAYER,
/// template slot indexes reset).
void settingsScreenOpened(Player& player, game::Backpack& backpack);
/// `{"action":"setting", "what":..}` (see SettingsScreen.cpp). Returns whether it was accepted.
bool handleSettingsScreenAction(Player& player, game::Backpack& backpack, nlohmann::json const& body);

} // namespace abp::feature::gui
