#pragma once

// Client settings of the native backpack screen: the client config (`Config.CLIENT`), in
// `config/client_gui.json` of the client mod.

#include <string>

#include "core/screen/StorageLayout.h"

namespace abp::client::ui {

struct ClientGuiConfig {
    int version = 1;
    /// `sortButtonsPosition`: "title_line_right" (default), "below_upgrades",
    /// "below_upgrade_tabs" or "hidden".
    std::string sortButtonsPosition = "title_line_right";
    /// `playButtonSound`: click sound when the screen's buttons are clicked.
    bool playButtonSound = true;
};

[[nodiscard]] ClientGuiConfig const& clientGuiConfig();
[[nodiscard]] core::screen::SortButtonsPosition sortButtonsPosition();

/// The UI click of ButtonBase.mouseClicked (when `playButtonSound` is on).
void playButtonClickSound();

} // namespace abp::client::ui
