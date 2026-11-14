#pragma once

// Magnet internals shared by the upgrade and its selftests.

#include <cstdint>

#include "mc/world/level/BlockPos.h"

#include "ui/menu/FilterMenu.h"
#include "upgrade/magnet/Magnet.h"

class AABB;
class Actor;
class BlockSource;
class ExperienceOrb;
class ItemActor;

namespace abp::upgrade::magnet {

/// The XP value of an orb (0 if unknown).
[[nodiscard]] int orbValue(ExperienceOrb const& orb);
/// `new AABB(pos).inflate(range)`: the block at `pos` grown by `range` blocks on every side.
[[nodiscard]] AABB rangeBox(BlockPos const& pos, int range);
/// The block position of a carrier (Java `Entity.blockPosition()`: the block of the feet).
[[nodiscard]] BlockPos carrierPos(Player& player);

/// `canNotPickup`: blocked by the remote movement tags (`carried` = a carrier runs the
/// magnet, null carrier = placed backpack).
[[nodiscard]] bool isBlockedByTags(Actor& entity, bool carried);
/// The item eligibility without the filter: alive and not blocked by tags (any pickup delay).
[[nodiscard]] bool isItemPullable(ItemActor& item, bool carried);

/// Inserts into the `getInventoryForUpgradeProcessing` (the backpack, plus with Inception
/// the backpacks stored inside it). Returns how many were taken (stored or voided).
int insertForUpgrade(UpgradeContext const& ctx, core::ItemStackData const& item, Player* player);

/// `canFillStorageWithXp`: the backpack's fluid handler accepts liquid experience.
[[nodiscard]] bool canFillStorageWithXp(std::shared_ptr<game::Backpack> const& backpack);

/// One magnet run (`MagnetUpgradeWrapper.tick` without the cooldown check) around `pos`.
/// `player` is the carrier (null for a placed backpack). Returns the cooldown the mod would set.
int runMagnet(UpgradeContext const& ctx, Player* player, BlockSource& region, BlockPos const& pos);

/// `MagnetUpgradeWrapper.tick`: unless this magnet is still in its own cooldown at game
/// tick `now`, runs it (`runMagnet`) and starts the cooldown it returns. True if it ran.
bool tickWithCooldown(UpgradeContext const& ctx, Player* player, BlockSource& region, BlockPos const& pos, std::int64_t now);

/// Chest-menu fallback of the settings (filter editor plus the two toggles in the control row).
class MagnetMenu : public ui::FilterMenu {
public:
    static constexpr int kItemsButton = 5; ///< control-row index
    static constexpr int kXpButton    = 6;

    MagnetMenu(Player& player, UpgradeContext const& ctx, int slots, bool advanced);
    void build(Player& player) override;

private:
    UpgradeContext mCtx;
};

} // namespace abp::upgrade::magnet
