#ifdef ABP_CLIENT

// Native tab of the Stonecutter upgrade (StonecutterUpgradeTab + StonecutterRecipeControl):
// the shift-click target toggle at (3,24) and the 81x108 recipe control, also at (3,24), whose input
// slot sits to the right of the toggle, centred over the 81x56 recipe list (4x3 recipes of 16x18,
// in the server's order, scroll track on the right), with the output in a 26x26 result frame below.

#include <algorithm>

#include "mc/world/item/ItemStack.h"

#include "client/Client.h"
#include "client/ui/screen/Tabs.h"
#include "client/ui/tabs/crafting/StationWidgets.h"
#include "client/ui/widgets/AbpStyle.h"

namespace abp::client::ui::station {

namespace {

// StonecutterRecipeControl geometry.
constexpr float kControlW        = 81;
constexpr float kControlH        = 108;
constexpr float kListYOffset     = 22;
constexpr float kInputSlotHeight = 18;
constexpr float kSpacing         = 4;
constexpr float kListH           = 56;
constexpr float kScrollTrackX    = 67; ///< scroll track inside the list background
constexpr float kSliderH         = 15;
constexpr float kSliderTravel    = 39;
constexpr float kScrollAreaH     = 54;
constexpr int   kVisibleRecipes  = 12;

/// Offset that centres an element of width `w` in the control (integer division
/// WidgetBase.getCenteredX).
constexpr float centred(int w) { return static_cast<float>((static_cast<int>(kControlW) - w) / 2); }

struct StonecutterData {
    ItemStack              input;
    ItemStack              output;
    std::vector<ItemStack> recipes;
    int                    selected         = -1;
    bool                   shiftIntoStorage = true;
};

/// The recipe list with its scroll bar (StonecutterRecipeControl).
class RecipeControl final : public Widget {
public:
    StonecutterData const*         data = nullptr;
    std::function<void(int index)> onSelect;
    int                            offset   = 0;
    float                          progress = 0;

    [[nodiscard]] bool hasInput() const { return data && !data->input.isNull(); }
    [[nodiscard]] int  count() const { return data ? static_cast<int>(data->recipes.size()) : 0; }
    [[nodiscard]] bool canScroll() const { return hasInput() && count() > kVisibleRecipes; }
    [[nodiscard]] int  hiddenRows() const { return (count() + 4 - 1) / 4 - 3; }

    [[nodiscard]] float listTop() const { return rect.y + kListYOffset; }

    /// Recipe index under (x, y): clicks are tested from (x + 1, top + 1), tooltips from (x, top + 2).
    [[nodiscard]] int recipeAt(float x, float y, float colOffset, float rowOffset) const {
        if (!hasInput()) return -1;
        for (int i = offset; i < offset + kVisibleRecipes && i < count(); ++i) {
            int   v  = i - offset;
            float rx = rect.x + colOffset + static_cast<float>(v % 4 * 16);
            float ry = listTop() + rowOffset + static_cast<float>(v / 4 * 18);
            if (x >= rx && x < rx + 16 && y >= ry && y < ry + 18) return i;
        }
        return -1;
    }

    void render(Painter& p, float mx, float my, double) override {
        // Input slot frame, list with its scroll track, result frame, slider, then the recipes.
        skin::slotsBackground(p, rect.x + centred(18), rect.y, 1, 1, 0);
        listBackground(p, {rect.x, listTop(), kScrollTrackX - 1, kListH});
        scrollBackground(p, {rect.x + kScrollTrackX, listTop(), kControlW - kScrollTrackX, kListH});
        resultSlotFrame(p, rect.x + centred(26), rect.y + kInputSlotHeight + kSpacing + kListH + kSpacing);
        slider(p, rect.x + kScrollTrackX + 1, listTop() + static_cast<float>(static_cast<int>(kSliderTravel * progress) + 1),
               canScroll());
        if (!hasInput()) return;
        for (int i = offset; i < offset + kVisibleRecipes && i < count(); ++i) {
            int      v  = i - offset;
            float    rx = rect.x + 1 + static_cast<float>(v % 4 * 16);
            float    ry = listTop() + static_cast<float>(v / 4 * 18) + 2;
            RecipeBg bg = RecipeBg::Normal;
            if (i == data->selected) bg = RecipeBg::Selected;
            else if (mx >= rx && my >= ry && mx < rx + 16 && my < ry + 18) bg = RecipeBg::Hovered;
            recipeBackground(p, rx, ry - 1, bg);
        }
        for (int i = offset; i < offset + kVisibleRecipes && i < count(); ++i) {
            int         v  = i - offset;
            float       rx = rect.x + 1 + static_cast<float>(v % 4 * 16);
            float       ry = listTop() + static_cast<float>(v / 4 * 18) + 2;
            auto const& s  = data->recipes[static_cast<std::size_t>(i)];
            if (!s.isNull()) p.item(s, rx, ry); // the stonecutter list shows no counts
        }
    }

    bool mouseDown(MouseButton, float x, float y) override {
        mDragging = false;
        if (!hovered(x, y)) return false;
        if (hasInput()) {
            if (int i = recipeAt(x, y, 1, 1); i >= 0) {
                if (onSelect) onSelect(i);
                return true;
            }
            float sx = rect.x + 1 + kScrollTrackX;
            float sy = listTop() + 1;
            if (x >= sx && x < sx + 12 && y >= sy && y < sy + kScrollAreaH) {
                mDragging = true;
                return true;
            }
        }
        return listArea().contains(x, y);
    }

    bool mouseUp(MouseButton, float, float) override {
        mDragging = false;
        return false;
    }

    bool mouseMove(float, float y) override {
        if (!mDragging || !canScroll()) return false;
        progress = std::clamp((y - listTop() - kSliderH / 2.f) / (kScrollAreaH - kSliderH), 0.f, 1.f);
        offset   = static_cast<int>(progress * static_cast<float>(hiddenRows()) + 0.5f) * 4;
        return true;
    }

    bool scroll(float x, float y, float delta) override {
        if (!hovered(x, y)) return false;
        if (canScroll()) scrollBy(delta);
        return true;
    }

    void tooltip(float x, float y, std::vector<TooltipLine>& out) const override {
        if (int i = recipeAt(x, y, 0, 2); i >= 0) {
            auto l = itemTooltipLines(data->recipes[static_cast<std::size_t>(i)]);
            out.insert(out.end(), l.begin(), l.end());
        }
    }

    /// After a data update: no input resets the scroll; a selection below the visible rows scrolls
    /// down just far enough to show it.
    void inventoryUpdated() {
        if (!hasInput()) {
            progress = 0;
            offset   = 0;
            return;
        }
        if (data->selected - offset >= kVisibleRecipes) {
            int rows = (data->selected - offset - kVisibleRecipes) / 4 + 1;
            scrollBy(static_cast<float>(-rows));
        }
    }

private:
    [[nodiscard]] Rect listArea() const { return {rect.x, listTop(), kControlW, kListH}; }

    void scrollBy(float delta) {
        int hidden = hiddenRows();
        if (hidden <= 0) return;
        progress = std::clamp(progress - delta / static_cast<float>(hidden), 0.f, 1.f);
        offset   = static_cast<int>(progress * static_cast<float>(hidden) + 0.5f) * 4;
    }

    bool mDragging = false;
};

class StonecutterTab final : public NativeTab {
public:
    Size build(TabContext const& ctx, Rect area, WidgetGroup& widgets) override {
        mControl = nullptr; // widgets of an earlier build are gone (NativeTab contract)
        reportTabOpen(ctx, widgets);
        auto  send = ctx.sendTabAction;
        float x0 = area.x, y0 = area.y;

        auto& shift   = widgets.add(std::make_unique<ToggleButton>());
        shift.size    = Button::Size::Normal;
        shift.rect    = {x0 + 3, y0 + 24, 18, 18};
        shift.states  = {
            {[](Painter& p, float x, float y) { icon(p, Icon::IntoBackpack, x + 1, y + 1); },
             lines(ctr("abp.crafting.shift_click_into_backpack", "Shift Click Result Into\nBackpack"))},
            {[](Painter& p, float x, float y) { icon(p, Icon::IntoInventory, x + 1, y + 1); },
             lines(ctr("abp.crafting.shift_click_into_inventory", "Shift Click Result Into\nPlayer's Inventory"))},
        };
        shift.current = [this] { return mData.shiftIntoStorage ? 0 : 1; };
        shift.onClick = [this, send](int) {
            mData.shiftIntoStorage = !mData.shiftIntoStorage;
            if (send) send({{"toggle", "shiftTarget"}});
        };

        // The control shares the toggle's origin; its list starts below the toggle, so the toggle
        // area is not claimed by the control (mouseDown returns false there).
        float cx = x0 + 3, cy = y0 + 24;
        auto& control    = widgets.add(std::make_unique<RecipeControl>());
        control.rect     = {cx, cy, kControlW, kControlH};
        control.data     = &mData;
        control.onSelect = [this, send](int recipe) {
            mData.selected = recipe; // shown at once, confirmed by the next server update
            if (send) send({{"recipe", recipe}});
        };
        mControl = &control;

        auto& input   = widgets.add(std::make_unique<ItemSlot>());
        input.frame   = false; // the control draws the frame
        input.rect    = {cx + centred(16) - 1, cy, 18, 18};
        input.item    = [this] { return &mData.input; };
        input.onClick = [send](int button, bool shift) {
            if (send) send({{"input", true}, {"button", button}, {"shift", shift}});
        };
        auto& output   = widgets.add(std::make_unique<ItemSlot>());
        output.frame   = false;
        output.rect    = {cx + centred(16) - 1, cy + kInputSlotHeight + kSpacing + kListH + kSpacing + 4, 18, 18};
        output.item    = [this] { return &mData.output; };
        output.onClick = [send](int button, bool shift) {
            if (send) send({{"output", true}, {"button", button}, {"shift", shift}});
        };
        update(ctx);
        return {3 + kControlW + 6, 24 + kControlH + 6};
    }

    void update(TabContext const& ctx) override {
        auto const& t = ctx.upgrade.tab;
        mData.input   = stackFromJson(t.value("input", nlohmann::json()));
        mData.output  = stackFromJson(t.value("output", nlohmann::json()));
        mData.recipes.clear();
        if (auto r = t.find("recipes"); r != t.end() && r->is_array()) {
            for (auto const& e : *r) mData.recipes.push_back(stackFromJson(e));
        }
        mData.selected         = t.value("selected", -1);
        mData.shiftIntoStorage = t.value("shiftIntoStorage", true);
        if (mControl) mControl->inventoryUpdated();
    }

private:
    StonecutterData mData;
    RecipeControl*  mControl = nullptr;
};

} // namespace

} // namespace abp::client::ui::station

ABP_REGISTER_NATIVE_TAB("stonecutter", [] { return std::make_unique<abp::client::ui::station::StonecutterTab>(); });

#endif // ABP_CLIENT
