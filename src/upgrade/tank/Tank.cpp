// Tank upgrade ("Tank Upgrade"; `TankUpgradeItem` / `TankUpgradeWrapper`).
//
// Settings JSON (travels with the upgrade item):
//   "tank":      {"fluid": "<phase-1 fluid id>", "amount": <mB>}
//   "in", "out": ItemStackData (INPUT_SLOT / OUTPUT_SLOT, one container each)

#include "upgrade/tank/Tank.h"

#include <algorithm>
#include <climits>
#include <cmath>

#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"

#include "api/BackpackCapabilities.h"
#include "api/Service.h"
#include "core/Capacity.h"
#include "core/FluidRegistry.h"
#include "game/ItemConv.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Module.h"
#include "net/NativeScreens.h"
#include "upgrade/UpgradeManager.h"
#include "upgrade/magnet/Magnet.h"
#include "upgrade/tank/Columns.h"
#include "upgrade/tank/FastTick.h"
#include "upgrade/tank/MenuBits.h"

namespace abp::upgrade::tank {

namespace fl = core::fluid;

/// `config/tank.json` (TankUpgradeConfig, `tankUpgrade` server config).
struct TankConfig {
    int version = 2;
    /// Capacity in mB the tank upgrade will have per row of storage slots.
    int capacityPerSlotRow = 4000;
    /// Ratio applied to the inventory stack multiplier before it multiplies the tank capacity
    /// (0 = stack upgrades don't change the capacity).
    double stackMultiplierRatio = 1.0;
    /// Cooldown (ticks) between fill / drain actions on the containers in the tank slots.
    int autoFillDrainContainerCooldown = 20;
    /// mB moved in / out per operation per row of storage slots (times the stack multiplier, at
    /// least a bucket).
    int maxInputOutput = 20;
};

namespace {
constexpr char const* kIn        = "in";
constexpr char const* kOut       = "out";
constexpr int         kSlotLimit = 1; ///< TankIOSlot.getMaxStackSize / TankComponentItemHandler.getSlotLimit

TankConfig const& cfg() { return config::get<TankConfig>(); }
} // namespace

// ---- sizing ---------------------------------------------------------------------------------------

int baseCapacity(game::Backpack& backpack) {
    return core::capacity::tankCapacity(api::backpack::slotRows(backpack), 1.0, std::clamp(cfg().capacityPerSlotRow, 500, 20000), 0.0);
}

int capacity(game::Backpack& backpack) {
    return core::capacity::tankCapacity(
        api::backpack::slotRows(backpack),
        api::backpack::stackMultiplier(backpack),
        std::clamp(cfg().capacityPerSlotRow, 500, 20000),
        std::clamp(cfg().stackMultiplierRatio, 0.0, 5.0)
    );
}

int maxInOut(game::Backpack& backpack) {
    return core::capacity::tankMaxInOut(
        api::backpack::slotRows(backpack),
        api::backpack::stackMultiplier(backpack),
        std::clamp(cfg().maxInputOutput, 1, 1000),
        std::clamp(cfg().stackMultiplierRatio, 0.0, 5.0)
    );
}

// ---- tank access ----------------------------------------------------------------------------------

bool isTank(UpgradeContext const& ctx) {
    auto const& slots = ctx.backpack().data().upgrades;
    if (ctx.slot() < 0 || ctx.slot() >= static_cast<int>(slots.size())) return false;
    return slots[static_cast<std::size_t>(ctx.slot())].item.id == kItemId;
}

std::vector<UpgradeContext> tanks(std::shared_ptr<game::Backpack> const& backpack) {
    std::vector<UpgradeContext> out;
    if (!backpack) return out;
    auto const& slots = backpack->data().upgrades;
    for (int i = 0; i < static_cast<int>(slots.size()); ++i) {
        if (slots[static_cast<std::size_t>(i)].item.id == kItemId) out.emplace_back(backpack, i);
    }
    return out;
}

fl::Tank read(UpgradeContext const& ctx) {
    if (!isTank(ctx)) return {};
    auto const& s = ctx.settings();
    return fl::Tank::fromJson(s.is_object() ? s.value("tank", nlohmann::json::object()) : nlohmann::json::object(), capacity(ctx.backpack()));
}

void write(UpgradeContext const& ctx, fl::Tank const& tank) {
    if (!isTank(ctx)) return;
    auto& s = ctx.settings();
    if (!s.is_object()) s = nlohmann::json::object();
    s["tank"] = tank.toJson();
    ctx.saveSettings();
}

api::FluidStack contents(UpgradeContext const& ctx) { return core::toApi(read(ctx).contents()); }

namespace {
void writeContents(UpgradeContext const& ctx, api::FluidStack const& stack) {
    write(ctx, fl::Tank(core::fromApi(stack), capacity(ctx.backpack())));
}
} // namespace

int fillTank(UpgradeContext const& ctx, api::FluidStack const& resource, bool simulate, bool ignoreLimit) {
    if (resource.empty() || !isTank(ctx)) return 0;
    int  cap = capacity(ctx.backpack());
    auto c   = contents(ctx);
    int  have = c.empty() ? 0 : c.amount;
    if (have >= cap || (!c.empty() && !c.isSameFluid(resource))) return 0;
    int toFill = std::min(cap - have, resource.amount);
    if (!ignoreLimit) toFill = std::min(maxInOut(ctx.backpack()), toFill);
    if (toFill <= 0) return 0;
    if (!simulate) writeContents(ctx, c.empty() ? resource.copyWithAmount(toFill) : c.copyWithAmount(have + toFill));
    return toFill;
}

api::FluidStack drainTank(UpgradeContext const& ctx, int maxDrain, bool simulate, bool ignoreLimit) {
    if (!isTank(ctx) || maxDrain <= 0) return {};
    auto c = contents(ctx);
    if (c.empty()) return {};
    int toDrain = std::min(maxDrain, c.amount);
    if (!ignoreLimit) toDrain = std::min(maxInOut(ctx.backpack()), toDrain);
    auto out = c.copyWithAmount(toDrain);
    if (!simulate) writeContents(ctx, c.copyWithAmount(c.amount - toDrain));
    return out;
}

namespace {
/// One tank upgrade as an API fluid handler (what pipes, pumps and other mods see).
class TankHandler final : public api::IFluidHandler {
public:
    TankHandler(UpgradeContext ctx, bool ignoreLimit) : mCtx(std::move(ctx)), mIgnoreLimit(ignoreLimit) {}

    [[nodiscard]] int             tanks() const override { return 1; }
    [[nodiscard]] api::FluidStack fluidInTank(int t) const override { return t == 0 ? contents(mCtx) : api::FluidStack{}; }
    [[nodiscard]] int tankCapacity(int t) const override { return t == 0 && isTank(mCtx) ? capacity(mCtx.backpack()) : 0; }
    [[nodiscard]] bool isFluidValid(int t, api::FluidStack const& s) const override {
        if (t != 0 || s.empty()) return false;
        auto c = contents(mCtx);
        return c.empty() || c.isSameFluid(s);
    }
    int fill(api::FluidStack const& resource, bool simulate) override { return fillTank(mCtx, resource, simulate, mIgnoreLimit); }
    api::FluidStack drain(api::FluidStack const& resource, bool simulate) override {
        if (resource.empty() || !contents(mCtx).isSameFluid(resource)) return {};
        return drainTank(mCtx, resource.amount, simulate, mIgnoreLimit);
    }
    api::FluidStack drainAny(int maxDrain, bool simulate) override { return drainTank(mCtx, maxDrain, simulate, mIgnoreLimit); }

private:
    UpgradeContext mCtx;
    bool           mIgnoreLimit;
};
} // namespace

ABP_REGISTER_BACKPACK_TANK(std::string(kId), [](UpgradeContext const& ctx) -> std::shared_ptr<api::IFluidHandler> {
    return std::make_shared<TankHandler>(ctx, false);
});

std::shared_ptr<api::IFluidHandler> storageHandler(std::shared_ptr<game::Backpack> const& backpack, bool ignoreLimit) {
    std::vector<std::shared_ptr<api::IFluidHandler>> parts;
    forEachInstalled(backpack, true, [&](IUpgrade& u, UpgradeContext const& ctx) {
        if (u.info().id == kId) parts.push_back(std::make_shared<TankHandler>(ctx, ignoreLimit));
    });
    if (parts.empty()) return nullptr;
    return std::make_shared<api::CombinedFluidHandler>(std::move(parts));
}

// ---- backpack-wide access by phase-1 id ------------------------------------------------------------

int fill(std::shared_ptr<game::Backpack> const& backpack, FluidStack const& stack, bool simulate, bool exact) {
    if (stack.empty()) return 0;
    auto all = tanks(backpack);
    if (exact && !simulate && fill(backpack, stack, true, false) < stack.amount) return 0;
    int left = stack.amount;
    for (int pass = 0; pass < 2 && left > 0; ++pass) {
        for (auto const& ctx : all) {
            if (left <= 0) break;
            auto t = read(ctx);
            if ((pass == 0) == t.empty()) continue;
            int n = t.fill({stack.id, left}, simulate);
            if (n <= 0) continue;
            left -= n;
            if (!simulate) write(ctx, t);
        }
    }
    int done = stack.amount - left;
    if (exact && done < stack.amount) return 0;
    return done;
}

FluidStack drain(std::shared_ptr<game::Backpack> const& backpack, std::string_view fluid, int max, bool simulate, bool exact) {
    if (max <= 0) return {};
    auto        all = tanks(backpack);
    std::string id(fluid);
    if (id.empty()) {
        for (auto const& ctx : all) {
            auto t = read(ctx);
            if (!t.empty()) {
                id = t.fluid();
                break;
            }
        }
        if (id.empty()) return {};
    }
    if (exact && !simulate && drain(backpack, id, max, true, false).amount < max) return {};
    int left = max;
    for (auto const& ctx : all) {
        if (left <= 0) break;
        auto t   = read(ctx);
        auto got = t.drain(id, left, simulate);
        if (got.empty()) continue;
        left -= got.amount;
        if (!simulate) write(ctx, t);
    }
    int done = max - left;
    if (done <= 0 || (exact && done < max)) return {};
    return {id, done};
}

long long amountOf(std::shared_ptr<game::Backpack> const& backpack, std::string_view fluid) {
    long long n = 0;
    for (auto const& ctx : tanks(backpack)) {
        auto t = read(ctx);
        if (!t.empty() && t.fluid() == fluid) n += t.amount();
    }
    return n;
}

std::vector<std::string> fluids(std::shared_ptr<game::Backpack> const& backpack) {
    std::vector<std::string> out;
    for (auto const& ctx : tanks(backpack)) {
        auto t = read(ctx);
        if (!t.empty() && std::find(out.begin(), out.end(), t.fluid()) == out.end()) out.push_back(t.fluid());
    }
    return out;
}

// ---- swap containers by item data -----------------------------------------------------------------

namespace {
core::ItemStackData itemData(std::string_view id, int aux) {
    auto stack = game::ItemConv::make(id, 1, aux);
    if (stack.isNull()) return {};
    auto d  = game::ItemConv::toData(stack);
    d.count = 1;
    return d;
}
} // namespace

std::optional<core::ItemStackData> emptyContainer(core::ItemStackData const& filled, FillFn const& fill, bool simulate) {
    if (filled.empty() || !fill) return std::nullopt;
    auto contents = api::fluidRegistry().contentsOf(filled.id, filled.aux);
    if (!contents) return std::nullopt;
    auto empty = itemData(contents->emptyItem, 0);
    if (empty.empty()) return std::nullopt;
    auto fluid = core::fromApi(contents->fluid);
    if (fill(fluid, true) < fluid.amount) return std::nullopt;
    if (!simulate && fill(fluid, false) < fluid.amount) return std::nullopt;
    return empty;
}

std::optional<core::ItemStackData>
fillContainer(core::ItemStackData const& empty, std::string_view fluid, DrainFn const& drain, bool simulate) {
    if (empty.empty() || fluid.empty() || !drain) return std::nullopt;
    auto filled = api::fluidRegistry().fill(empty.id, core::toApi({std::string(fluid), 1}));
    if (!filled) return std::nullopt;
    auto item = itemData(filled->item, filled->aux);
    if (item.empty()) return std::nullopt;
    if (drain(fluid, filled->amount, true).amount < filled->amount) return std::nullopt;
    if (!simulate && drain(fluid, filled->amount, false).amount < filled->amount) return std::nullopt;
    return item;
}

FillFn fillerOf(UpgradeContext const& ctx) {
    return [ctx](FluidStack const& s, bool simulate) {
        auto t = read(ctx);
        if (!isTank(ctx) || t.fill(s, true, true) < s.amount) return 0;
        if (simulate) return s.amount;
        int n = t.fill(s, false, true);
        write(ctx, t);
        return n;
    };
}

DrainFn drainerOf(UpgradeContext const& ctx) {
    return [ctx](std::string_view fluid, int amount, bool simulate) -> FluidStack {
        auto t = read(ctx);
        if (!isTank(ctx)) return {};
        auto got = t.drain(fluid, amount, simulate, true);
        if (!simulate && !got.empty()) write(ctx, t);
        return got;
    };
}

// ---- the container logic (TankUpgradeWrapper.tick / fillHandler / drainHandler) -------------

namespace {
using HandlerPtr = std::shared_ptr<api::IFluidHandlerItem>;
using Update     = std::function<void(ItemStack const&)>;

/// The item's fluid handler. Like NeoForge item fluid handlers, it only
/// works on a single item: a stack of several gets none.
HandlerPtr handlerOf(ItemStack const& stack) {
    if (stack.isNull() || stack.mCount != 1) return nullptr;
    return api::service().itemFluid(stack);
}

/// The handler of the container stored in a tank slot (none unless the slot holds exactly one).
HandlerPtr slotHandler(core::ItemStackData const& item) {
    if (item.empty() || item.count != 1) return nullptr;
    return handlerOf(game::ItemConv::toStack(item, 1));
}

core::ItemStackData dataOf(ItemStack const& s) { return s.isNull() ? core::ItemStackData{} : game::ItemConv::toData(s); }

/// TankUpgradeWrapper's container helpers, bound to one tank.
class Logic {
public:
    explicit Logic(UpgradeContext ctx) : mCtx(std::move(ctx)) {}

    [[nodiscard]] api::FluidStack current() const { return contents(mCtx); }

    /// isValidFluidHandler: an output container must have an empty tank or one holding the tank's
    /// fluid (only when the tank is not empty); an input container must hold a fluid the tank can
    /// take (any fluid while the tank is empty).
    [[nodiscard]] bool isValidFluidHandler(api::IFluidHandler const& h, bool isOutput) const {
        auto c         = current();
        bool tankEmpty = c.empty();
        for (int t = 0; t < h.tanks(); ++t) {
            auto in = h.fluidInTank(t);
            if (isOutput && (in.empty() || (!tankEmpty && in.isSameFluid(c)))) return true;
            if (!isOutput && !in.empty() && (tankEmpty || c.isSameFluid(in))) return true;
        }
        return false;
    }

    /// isValidFluidItem (TankComponentItemHandler.isItemValid): checked on one item of `item`.
    [[nodiscard]] bool isValidFluidItem(core::ItemStackData const& item, bool isOutput) const {
        if (item.empty()) return false;
        auto h = handlerOf(game::ItemConv::toStack(item, 1));
        return h && isValidFluidHandler(*h, isOutput);
    }

    /// fillHandler: up to a bucket of the tank's fluid (within the per-operation limit) goes into
    /// the container.
    bool fillHandler(api::IFluidHandlerItem& fh, Update const& update) {
        auto c = current();
        if (c.empty() || !isValidFluidHandler(fh, true)) return false;
        int filled = fh.fill(c.copyWithAmount(std::min(api::kBucketVolume, c.amount)), true);
        if (filled <= 0) return false;
        auto drained = drainTank(mCtx, filled, false, false);
        fh.fill(drained, false);
        update(fh.container());
        return true;
    }

    /// drainHandler: up to a bucket (or what still fits) comes out of the container into the tank
    /// (within the per-operation limit).
    bool drainHandler(api::IFluidHandlerItem& fh, Update const& update) {
        if (!isValidFluidHandler(fh, false)) return false;
        auto            c = current();
        api::FluidStack extracted =
            c.empty() ? fh.drainAny(api::kBucketVolume, true)
                      : fh.drain(c.copyWithAmount(std::min(api::kBucketVolume, capacity(mCtx.backpack()) - c.amount)), true);
        if (extracted.empty()) return false;
        int  filled    = fillTank(mCtx, extracted, false, false);
        auto toExtract = filled == extracted.amount ? extracted : extracted.copyWithAmount(filled);
        fh.drain(toExtract, false);
        update(fh.container());
        return true;
    }

    /// One tick step: the container in the input slot is drained into the tank, the one in the
    /// output slot is filled from it; each container stays in its slot. When the
    /// output slot holds a fluid container its step alone decides the return value.
    bool processSlots() {
        bool did = false;
        if (auto h = slotHandler(fluidui::slotItem(mCtx, kIn))) {
            did = drainHandler(*h, [&](ItemStack const& s) { fluidui::setSlotItem(mCtx, kIn, dataOf(s)); });
        }
        if (auto h = slotHandler(fluidui::slotItem(mCtx, kOut))) {
            did = fillHandler(*h, [&](ItemStack const& s) { fluidui::setSlotItem(mCtx, kOut, dataOf(s)); });
        }
        return did;
    }

private:
    UpgradeContext mCtx;
};

struct Cooldown {
    std::int64_t until = 0;
};

/// Settings keys an earlier AlleviatedBackpacks layout used (two extra result slots, a format
/// version), and slots holding more than one container. The mod keeps one container in each of
/// the two slots: the extra items go into the backpack storage. What does not fit stays where it
/// is and is retried on later ticks, so nothing is lost.
constexpr char const* kLegacyKeys[] = {"inResult", "outResult"};
constexpr char const* kLegacyVersionKey = "slotsV";

core::ItemStackData moveToStorage(UpgradeContext const& ctx, core::ItemStackData const& item) {
    if (item.empty()) return {};
    auto&               inv = ctx.backpack().inventory();
    core::AccessContext access{core::Source::Internal};
    auto                sim = inv.insert(item, access, true);
    if (sim.voided > 0 || sim.inserted <= 0) return item;
    auto r    = inv.insert(item.withCount(sim.inserted), access, false);
    int  left = item.count - r.inserted - r.voided;
    return left > 0 ? item.withCount(left) : core::ItemStackData{};
}

void migrateLegacySlots(UpgradeContext const& ctx) {
    auto const& s = ctx.settings();
    if (!s.is_object()) return;
    bool legacy = s.contains(kLegacyVersionKey);
    for (auto const* key : kLegacyKeys) legacy |= s.contains(key);
    for (auto const* key : {kIn, kOut}) legacy |= fluidui::slotItem(ctx, key).count > 1;
    if (!legacy) return;
    for (auto const* key : kLegacyKeys) {
        auto item = fluidui::slotItem(ctx, key);
        if (!item.empty()) fluidui::setSlotItem(ctx, key, moveToStorage(ctx, item));
        else if (ctx.settings().contains(key)) fluidui::setSlotItem(ctx, key, {});
    }
    for (auto const* key : {kIn, kOut}) {
        auto item = fluidui::slotItem(ctx, key);
        if (item.count <= 1) continue;
        auto left = moveToStorage(ctx, item.withCount(item.count - 1));
        fluidui::setSlotItem(ctx, key, item.withCount(1 + (left.empty() ? 0 : left.count)));
    }
    auto& after = ctx.settings();
    if (after.is_object() && after.contains(kLegacyVersionKey)) {
        after.erase(std::string(kLegacyVersionKey));
        ctx.saveSettings();
    }
}

/// TankUpgradeWrapper.tick: one container step, then `autoFillDrainContainerCooldown`
/// ticks of rest whenever something moved.
void tickSlots(UpgradeContext const& ctx) {
    if (!isTank(ctx)) return;
    migrateLegacySlots(ctx);
    auto& cd = ctx.transient<Cooldown>();
    if (fasttick::now() < cd.until) return;
    if (Logic(ctx).processSlots()) cd.until = fasttick::now() + std::clamp(cfg().autoFillDrainContainerCooldown, 1, 100);
}
} // namespace

bool processSlots(UpgradeContext const& ctx) {
    if (!isTank(ctx)) return false;
    return Logic(ctx).processSlots();
}

std::optional<ItemStack> interactWithCursor(UpgradeContext const& ctx, ItemStack const& cursor) {
    if (!isTank(ctx)) return std::nullopt;
    auto h = handlerOf(cursor);
    if (!h) return std::nullopt;
    Logic                    logic(ctx);
    std::optional<ItemStack> result;
    Update                   update = [&](ItemStack const& s) { result = s; };
    // TankClickPayload: an empty tank takes the container's fluid; otherwise the container
    // is filled, or drained when it can't be filled.
    if (logic.current().empty()) {
        logic.drainHandler(*h, update);
    } else if (!logic.fillHandler(*h, update)) {
        logic.drainHandler(*h, update);
    }
    return result;
}

// ---- display ------------------------------------------------------------------------------------

namespace {
std::string withSeparators(long long n) {
    auto s     = std::to_string(n < 0 ? -n : n);
    int  first = static_cast<int>(s.size()) % 3;
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (i != 0 && (static_cast<int>(i) - first) % 3 == 0) out += ',';
        out += s[i];
    }
    return n < 0 ? "-" + out : out;
}

template <class... Args>
std::string text(Player const* p, std::string_view key, std::string_view fallback, Args const&... args) {
    return p ? trFor(*p, key, fallback, args...) : tr(key, fallback, args...);
}
} // namespace

std::string fluidName(Player const* player, std::string_view fluid) {
    auto fam = fl::family(fluid);
    if (fam == "potion") return text(player, "abp.tank.fluid.potion", "Potion #{}", fl::potionAux(fluid));
    if (fam == "splash_potion") return text(player, "abp.tank.fluid.splash_potion", "Splash Potion #{}", fl::potionAux(fluid));
    if (fam == "lingering_potion") return text(player, "abp.tank.fluid.lingering_potion", "Lingering Potion #{}", fl::potionAux(fluid));
    if (fluid == fl::kWater) return text(player, "abp.tank.fluid.water", "Water");
    if (fluid == fl::kLava) return text(player, "abp.tank.fluid.lava", "Lava");
    if (fluid == fl::kMilk) return text(player, "abp.tank.fluid.milk", "Milk");
    if (fluid == fl::kPowderSnow) return text(player, "abp.tank.fluid.powder_snow", "Powder Snow");
    if (fluid == fl::kHoney) return text(player, "abp.tank.fluid.honey", "Honey");
    if (fluid == fl::kExperience) return text(player, "abp.tank.fluid.experience", "Experience");
    // A mod fluid: its registered name.
    auto api = core::toApi({std::string(fluid), 1});
    if (auto const* info = api::fluidRegistry().fluid(api.id)) return text(player, info->nameKey, info->nameFallback);
    return std::string(fluid);
}

std::string describe(Player const* player, fl::Tank const& tank) {
    if (tank.empty()) return text(player, "abp.tank.empty", "§7Empty §8({} mB)", withSeparators(tank.capacity()));
    return text(
        player,
        "abp.tank.contents",
        "§b{}: §f{} §7/ {} mB",
        fluidName(player, tank.fluid()),
        withSeparators(tank.amount()),
        withSeparators(tank.capacity())
    );
}

// ---- the upgrade --------------------------------------------------------------------------------

namespace {

bool alive(UpgradeContext const& ctx) { return isTank(ctx); }

std::shared_ptr<ui::Menu> slotsMenu(Player& player, UpgradeContext const& ctx) {
    // TankUpgradeContainer: the input and output TankIOSlots.
    auto factory = [ctx]() {
        auto                                   isAlive = [ctx] { return alive(ctx); };
        std::vector<std::shared_ptr<ui::Slot>> slots;
        auto valid = [ctx](bool output) {
            return [ctx, output](core::ItemStackData const& d) {
                return !fluidui::isOwnBackpack(ctx, d) && Logic(ctx).isValidFluidItem(d, output);
            };
        };
        slots.push_back(std::make_shared<fluidui::SettingsItemSlot>(ctx, kIn, valid(false), kSlotLimit, isAlive));
        slots.push_back(std::make_shared<fluidui::SettingsItemSlot>(ctx, kOut, valid(true), kSlotLimit, isAlive));
        return slots;
    };
    return std::make_shared<fluidui::ItemSlotsMenu>(trFor(player, "abp.tank.tab", "Tank"), ctx, factory, [ctx] { return alive(ctx); });
}

int otherTanks(UpgradeContext const& ctx) {
    int         n     = 0;
    auto const& slots = ctx.backpack().data().upgrades;
    for (int i = 0; i < static_cast<int>(slots.size()); ++i) {
        if (i != ctx.slot() && slots[static_cast<std::size_t>(i)].item.id == kItemId) ++n;
    }
    return n;
}

class TankUpgrade : public IUpgrade {
public:
    [[nodiscard]] UpgradeInfo const& info() const override {
        static UpgradeInfo const kInfo{
            .id             = std::string(kId),
            .itemId         = std::string(kItemId),
            .nameKey        = "abp.tank.name",
            .nameFallback   = "Tank Upgrade",
            .group          = "tank",
            .maxPerBackpack = 1000, // The limit (two, an upgrade conflict) is checked in canInstallItem with its message
            .hasSettings    = true,
            .toggleable     = false,
        };
        return kInfo;
    }

    bool canInstallItem(UpgradeContext const& ctx, core::ItemStackData const& item, std::string& reason) const override {
        auto msg = [&](std::string_view key, std::string_view fallback, auto const&... args) {
            return ctx.player() ? trFor(*ctx.player(), key, fallback, args...) : tr(key, fallback, args...);
        };
        // TankUpgradeItem UPGRADE_CONFLICT_DEFINITIONS: at most one other tank.
        if (otherTanks(ctx) >= 2) {
            reason = msg("abp.tank.error.two_tanks", "There are already two Tank Upgrades in other upgrade slots");
            return false;
        }
        // TankUpgradeItem.checkExtraInsertConditions: the contents the item carries must fit.
        auto settings = settingsOf(item);
        int  amount   = 0;
        if (auto it = settings.find("tank"); it != settings.end() && it->is_object()) amount = std::max(0, it->value("amount", 0));
        int capacityAfter = std::max(1, capacity(ctx.backpack()));
        double required   = static_cast<double>(amount) / capacityAfter;
        if (required > 1) {
            reason = msg(
                "abp.tank.error.capacity_high",
                "Backpack can't support this tank upgrade. Upgrade backpack or add stack multiplier to get at least {}x higher capacity",
                core::capacity::formatRequiredMultiplier(required)
            );
            return false;
        }
        return columns::canInstall(ctx, item, reason);
    }

    void onInstall(UpgradeContext const& ctx) override {
        migrateLegacySlots(ctx);
        columns::sync(ctx.backpack());
    }
    void onRemove(UpgradeContext const& ctx) override { columns::sync(ctx.backpack(), ctx.slot()); }

    void attach(UpgradeContext const& ctx, core::BackpackInventory&) override {
        // Phase-1 backpacks had tanks without taken columns: take them when the items fit.
        if (columns::expected(ctx.backpack()) != ctx.backpack().data().columnsTaken) columns::sync(ctx.backpack());
    }

    void onTick(TickContext const& tick) override {
        fasttick::keepAlive(tick, "tank", [](Player&, UpgradeContext const& ctx, game::ItemLocation, int) { tickSlots(ctx); });
    }

    void onPlacedTick(PlacedTickContext const& tick) override { tickSlots(tick.ctx); }

    std::shared_ptr<ui::Menu> createSettingsMenu(Player& player, UpgradeContext const& ctx) override { return slotsMenu(player, ctx); }
};

// ---- native screen: tab data, bar clicks, render info, contents ---------------------------------

nlohmann::json fluidJson(Player* player, api::FluidStack const& f) {
    if (f.empty()) return nullptr;
    auto const* info = api::fluidRegistry().fluid(f.id);
    return nlohmann::json{
        {"id",     f.id                                      },
        {"nbt",    f.nbt                                     },
        {"amount", f.amount                                  },
        {"name",   fluidName(player, core::legacyIdOf(f))     },
        {"color",  info ? info->color : 0xFFFFFFFFu          },
        {"xp",     api::fluidRegistry().hasTag(f.id, api::fluid_tags::Experience)},
    };
}

ABP_REGISTER_TAB_DATA(net::TabDataProvider{
    std::string(kId),
    [](Player& player, UpgradeContext const& ctx) {
        return nlohmann::json{
            {"fluidTank",    {{"fluid", fluidJson(&player, contents(ctx))}, {"capacity", capacity(ctx.backpack())}}},
            {"inventoryColumns", 2                                   },
            {"columnsTaken", ctx.backpack().data().columnsTaken      },
            {"tabLabel",     trFor(player, "abp.tank.tab", "Tank")   },
            {"tabTooltip",   trFor(player, "abp.tank.tab.tooltip", "Tank")},
        };
    }
});

/// TankClickPayload: a click on the tank bar with a fluid container on the cursor.
ABP_REGISTER_TAB_ACTION(net::TabActionHandler{
    std::string(kId),
    [](Player& player, UpgradeContext const& ctx, nlohmann::json const& data) {
        if (data.value("op", std::string{}) != "click") return false;
        if (auto result = interactWithCursor(ctx.withPlayer(&player), fluidui::cursorOf(player))) fluidui::setCursor(player, *result);
        return true;
    }
});

/// UpgradeHandler.initTankRenderInfoCallbacks: one tank renders right when it sits in the
/// second half of the upgrade slots, two tanks render left then right. Fill ratio in 0.1 steps.
ABP_REGISTER_RENDER_INFO([](game::Backpack& backpack, nlohmann::json& extra) {
    auto const& slots = backpack.data().upgrades;
    std::vector<int> tankSlots;
    bool             singleRight = false;
    int              minRight    = static_cast<int>(slots.size()) / 2;
    for (int i = 0; i < static_cast<int>(slots.size()); ++i) {
        if (slots[static_cast<std::size_t>(i)].item.id != kItemId) continue;
        tankSlots.push_back(i);
        if (i >= minRight) singleRight = true;
    }
    if (tankSlots.empty()) return;
    auto        self = backpack.shared_from_this();
    std::string pos  = tankSlots.size() == 1 && singleRight ? "right" : "left";
    for (int s : tankSlots) {
        UpgradeContext ctx(self, s);
        auto           c   = contents(ctx);
        int            cap = std::max(1, capacity(backpack));
        nlohmann::json info = nlohmann::json::object();
        if (!c.empty()) {
            auto const* fluid = api::fluidRegistry().fluid(c.id);
            info["fluid"]     = c.id;
            info["nbt"]       = c.nbt;
            info["color"]     = fluid ? fluid->color : 0xFFFFFFFFu;
            info["fillRatio"] = std::round(static_cast<float>(c.amount) / static_cast<float>(cap) * 10.f) / 10.f;
        }
        extra["tanks"][pos] = std::move(info);
        pos                 = "right";
    }
});

/// MagnetUpgradeWrapper.tryToFillTank: picked up experience orbs go into the backpack's
/// tanks as liquid experience.
class TankMagnetXp {
public:
    void install() {
        magnet::addXpConsumer(std::string(kOwner), 20, [](UpgradeContext const& magnetCtx, Player*, int amount) {
            auto h = api::backpack::fluidHandler(magnetCtx.backpackPtr());
            if (!h || amount <= 0) return 0;
            auto& caps   = api::service();
            int   liquid = api::xp::experienceToLiquid(static_cast<float>(amount));
            int   filled = api::fillTagged(caps, *h, api::fluid_tags::Experience, liquid, {std::string(api::fluids::Experience), liquid}, true);
            int   points = static_cast<int>(api::xp::liquidToExperience(filled));
            if (points <= 0) return 0;
            api::fillTagged(caps, *h, api::fluid_tags::Experience, api::xp::experienceToLiquid(static_cast<float>(points)), {std::string(api::fluids::Experience), 1}, false);
            return points;
        });
    }
    void uninstall() { magnet::removeXpConsumers(std::string(kOwner)); }

private:
    static constexpr std::string_view kOwner = "tank";
};

} // namespace
} // namespace abp::upgrade::tank

ABP_REGISTER_CONFIG("tank", abp::upgrade::tank::TankConfig);
ABP_REGISTER_UPGRADE(abp::upgrade::tank::TankUpgrade);
ABP_REGISTER_INVENTORY_COLUMNS("tank", 2);
ABP_REGISTER_MODULE(abp::upgrade::tank::TankMagnetXp);
