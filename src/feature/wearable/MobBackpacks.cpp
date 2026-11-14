// Backpacks on spawned mobs: the behaviour of EntityBackpackAdditionHandler (GPL-3.0
// baseline), with the settings of `alleviatedbackpacks-server.json` -> entityBackpackAdditions.
//
// - Spawn: a mob listed in `entityLootTableList` whose chest slot is empty gets a backpack with a
//   1 / (int)(1 / chance) chance, unless it is a raider of a raid. The tier is a fixed weighted
//   roll (leather 625, copper 250, iron 125, gold 25, diamond 5, netherite 1); at mid local
//   difficulty only copper..diamond, at high local difficulty only copper..gold can be rolled.
// - The tier's min difficulty d0 and difficulty d = max(d0, random 0..3) scale everything.
// - Its colours are the mob's spawn egg colours; with d > 0, loot from the mob's loot table is
//   added lazily on first open (one roll, keeping size * d / 3 of the stacks); 1 in 4 get a
//   jukebox upgrade (advanced 25%) with random music discs that plays from the mob.
// - The mob gets d + 2 random potion buffs for 30 minutes, + base max health * d0, and
//   tier-weighted helmet / leggings / boots (enchanted 1 in 6 - d0).
// - Drop: only to a player kill, chance (backpackDropChance + looting * lootingChanceIncrease) *
//   tier multiplier (1 / 1.25 / 1.5 / 3 / 4.5 / 6). Otherwise the backpack's storage goes away.
// - A mob leaving the level (including by converting into another mob) loses its backpack's
//   storage. A converted mob keeps wearing the backpack as plain equipment that never drops.
//
// Backpack contents are generated lazily (Storage create listener), so mobs that despawn cost
// nothing; only jukebox mobs get their storage at spawn (the jukebox must play).

#include <algorithm>
#include <bitset>
#include <cmath>
#include <exception>
#include <unordered_map>
#include <unordered_set>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/entity/MobDieEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/event/world/SpawnMobEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"

#include "mc/entity/components_json_legacy/ExplodeComponent.h"
#include "mc/entity/components_json_legacy/TransformationComponent.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/effect/MobEffectIds.h"

#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/shared_types/legacy/Difficulty.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include "mc/util/LootTableUtils.h"
#include "mc/util/Random.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/world/actor/ActorCategory.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/ai/village/Raid.h"
#include "mc/world/actor/ai/village/Village.h"
#include "mc/world/actor/ai/village/VillageManager.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/attribute/BaseAttributeMap.h"
#include "mc/world/attribute/SharedAttributes.h"
#include "mc/world/effect/EffectDuration.h"
#include "mc/world/effect/MobEffect.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/inventory/CraftingContainer.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/enchanting/Enchant.h"
#include "mc/world/item/enchanting/EnchantUtils.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionType.h"
#include "mc/world/phys/AABB.h"
#include "mc/world/level/BlockSource.h"

#include "nlohmann/json.hpp"

#include "core/WorldRules.h"
#include "feature/admin/AccessLog.h"
#include "feature/admin/Admin.h"
#include "feature/admin/AbpConfig.h"
#include "feature/wearable/Wearable.h"
#include "feature/wearable/WearableConfig.h"
#include "game/Backpack.h"
#include "game/BackpackItem.h"
#include "game/ItemConv.h"
#include "game/Storage.h"
#include "mod/Config.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "ui/menu/BackpackMenu.h"
#include "ui/menu/MenuManager.h"
#include "upgrade/UpgradeManager.h"
#include "upgrade/jukebox/Discs.h"
#include "upgrade/jukebox/Jukebox.h"

ABP_REGISTER_CONFIG("wearable", abp::wearable::WearableConfig);

namespace abp::wearable {

using SharedTypes::Legacy::ArmorSlot;

namespace {

constexpr int kPotionDurationTicks = 30 * 60 * 20; ///< 30 minutes
constexpr int kMaxDifficulty       = 3;            ///< MAX_DIFFICULTY

admin::AbpServerConfig::EntityBackpackAdditions const& additions() { return admin::abpServer().entityBackpackAdditions; }

int nextInt(int bound) { return bound <= 1 ? 0 : std::uniform_int_distribution<int>(0, bound - 1)(rng()); }
double uniform() { return std::uniform_real_distribution<double>(0.0, 1.0)(rng()); }

void setUserData(ItemStack& stack, std::function<void(CompoundTag&)> const& edit) {
    auto tag = stack.mUserData ? stack.mUserData->clone() : std::make_unique<CompoundTag>();
    edit(*tag);
    stack.setUserData(std::move(tag));
}

bool hasByte(ItemStack const& stack, std::string_view key) {
    return !stack.isNull() && stack.mUserData && stack.mUserData->contains(key, Tag::Type::Byte);
}

BaseAttributeMap& attributesOf(Mob& mob) { return const_cast<BaseAttributeMap&>(*static_cast<Actor&>(mob).getAttributes()); }

// ---- loot -----------------------------------------------------------------------------------------

void installRandomUpgrades(std::shared_ptr<game::Backpack> const& bp) {
    auto const&                     c = config::get<WearableConfig>().mobs;
    std::vector<upgrade::IUpgrade*> pool;
    for (auto const& u : upgrade::all()) {
        auto const& info = u->info();
        if (std::find(c.upgradeBlacklist.begin(), c.upgradeBlacklist.end(), info.id) != c.upgradeBlacklist.end()) continue;
        pool.push_back(u.get());
    }
    if (pool.empty()) return;
    for (int slot = 0; slot < static_cast<int>(bp->data().upgrades.size()); ++slot) {
        if (!bp->data().upgrades[static_cast<std::size_t>(slot)].empty()) continue;
        if (uniform() >= c.upgradeChancePerSlot) continue;
        std::shuffle(pool.begin(), pool.end(), rng());
        for (auto* u : pool) {
            auto stack = game::ItemConv::make(u->info().itemId);
            if (stack.isNull()) continue;
            auto item = game::ItemConv::toData(stack).withCount(1);
            if (!upgrade::canInstall(bp, slot, item, nullptr)) continue;
            if (upgrade::install(bp, slot, item, nullptr)) break;
        }
    }
}

/// BackpackWrapper.fillWithLootFromTable: one roll of the table, backpacks removed from
/// the result, then a random (int)(size * factor) of the stacks are kept (the factor is at most 1);
/// LootHelper.fillWithLoot then puts them into random empty slots (overflow is voided).
std::vector<core::ItemStackData> rollLoot(PendingLoot const& loot) {
    std::vector<core::ItemStackData> out;
    auto                             level = ll::service::getLevel();
    if (!level || loot.table.empty() || loot.lootFactor <= 0) return out;
    SuppressInjection noInject;
    auto              box = std::make_unique<CraftingContainer>(9, 3);
    Random            random(loot.seed, false);
    std::mt19937      pick(loot.seed);
    Util::LootTableUtils::fillContainer(*level, *box, random, "loot_tables/" + loot.table + ".json", DimensionType(loot.dimension), nullptr);
    for (int i = 0; i < box->getContainerSize(); ++i) {
        auto const& it = box->getItem(i);
        if (it.isNull() || game::isBackpack(it)) continue;
        out.push_back(game::ItemConv::toData(it));
    }
    box->removeAllItems();
    std::shuffle(out.begin(), out.end(), pick);
    auto keep = static_cast<std::size_t>(static_cast<float>(out.size()) * std::min(1.0f, loot.lootFactor));
    out.resize(std::min(keep, out.size()));
    return out;
}

void putIntoRandomEmptySlots(game::Backpack& bp, std::vector<core::ItemStackData> loot, unsigned seed) {
    auto&            inv = bp.inventory();
    std::vector<int> empty;
    for (int i = 0; i < inv.size(); ++i) {
        if (inv.at(i).empty()) empty.push_back(i);
    }
    std::mt19937 shuffle(seed ^ 0x5bd1e995u);
    std::shuffle(empty.begin(), empty.end(), shuffle);
    for (auto& stack : loot) {
        if (empty.empty()) {
            log().warn("wearable: too much loot to add to backpack {}; overflow is voided", bp.uuid());
            return;
        }
        if (stack.empty()) continue;
        inv.set(empty.back(), std::move(stack));
        empty.pop_back();
    }
}

// ---- spawn eggs --------------------------------------------------------------------------------------

struct EggColors {
    std::string_view type;
    std::uint32_t    background;
    std::uint32_t    highlight;
};

/// Vanilla spawn egg colours (Java SpawnEggItem; the same colours as Bedrock's eggs).
constexpr EggColors kEggColors[] = {
    {"minecraft:creeper",            0x0DA70B, 0x000000},
    {"minecraft:drowned",            0x8FF1D7, 0x799C65},
    {"minecraft:enderman",           0x161616, 0x000000},
    {"minecraft:evocation_illager",  0x959B9B, 0x1E1C1A},
    {"minecraft:husk",               0x797061, 0xE6CC94},
    {"minecraft:piglin",             0x995F40, 0xF9F3A4},
    {"minecraft:piglin_brute",       0x592A10, 0xF9F3A4},
    {"minecraft:pillager",           0x532F36, 0x959B9B},
    {"minecraft:skeleton",           0xC1C1C1, 0x494949},
    {"minecraft:stray",              0x617677, 0xDDEAEA},
    {"minecraft:vex",                0x7A90A4, 0xE8EDF1},
    {"minecraft:vindicator",         0x959B9B, 0x275E61},
    {"minecraft:witch",              0x340000, 0x51A03E},
    {"minecraft:wither_skeleton",    0x141414, 0x474D4D},
    {"minecraft:zombie",             0x00AFAF, 0x799C65},
    {"minecraft:zombie_villager_v2", 0x563C33, 0x799C65},
    {"minecraft:zombie_villager",    0x563C33, 0x799C65},
    {"minecraft:zombie_pigman",      0xEA9393, 0x4C7129},
    {"minecraft:bogged",             0x8A9C72, 0x314D1B},
    {"minecraft:spider",             0x342D27, 0xA80E0E},
    {"minecraft:cave_spider",        0x0C424E, 0xA80E0E},
    {"minecraft:blaze",              0xF6B201, 0xFFF87E},
    {"minecraft:ravager",            0x757470, 0x5B5049},
    {"minecraft:silverfish",         0x6E6E6E, 0x303030},
    {"minecraft:slime",              0x51A03E, 0x7EBF6E},
};

// ---- equipment ----------------------------------------------------------------------------------------

/// APPLICABLE_EFFECTS: one of resistance / regeneration (max amplifier 1), fire resistance,
/// absorption, health boost, speed, strength.
struct ApplicableEffect {
    std::vector<std::string_view> effects;
    int                           maxAmplifier;
};

void applyPotions(Mob& mob, int difficulty, int minDifficulty) {
    if (!additions().buffWithPotionEffects) return;
    static std::vector<ApplicableEffect> const kEffects{
        {{"resistance", "regeneration"}, 1},
        {{"fire_resistance"}, 1 << 30},
        {{"absorption"}, 1 << 30},
        {{"health_boost"}, 1 << 30},
        {{"speed"}, 1 << 30},
        {{"strength"}, 1 << 30},
    };
    auto pool = kEffects;
    std::shuffle(pool.begin(), pool.end(), rng());
    auto n = std::min<std::size_t>(pool.size(), static_cast<std::size_t>(difficulty + 2));
    for (std::size_t i = 0; i < n; ++i) {
        auto const& e         = pool[i];
        int         amplifier = std::min(std::max(minDifficulty, nextInt(difficulty + 1)), e.maxAmplifier);
        auto        name      = e.effects[static_cast<std::size_t>(nextInt(static_cast<int>(e.effects.size())))];
        auto const* effect    = MobEffect::getByName(std::string(name));
        if (!effect) continue;
        MobEffectInstance instance(effect->mId, EffectDuration{kPotionDurationTicks});
        instance.mAmplifier = amplifier;
        mob.addEffect(instance);
    }
}

/// Base max health without effects / modifiers (maxHealth.getBaseValue()).
float baseMaxHealth(Mob& mob) {
    auto ref = mob.getAttribute(SharedAttributes::HEALTH());
    return ref.mPtr ? ref.mPtr->mDefaultMaxValue : static_cast<float>(mob.getMaxHealth());
}

/// raiseHealth: max health + base * minDifficulty (when that is > 0.1), then full health.
void raiseHealth(Mob& mob, int minDifficulty) {
    if (!additions().buffHealth) return;
    float base     = baseMaxHealth(mob);
    float addition = base * static_cast<float>(minDifficulty);
    auto& attrs    = attributesOf(mob);
    if (addition > 0.1f) (void)attrs.setMaxValue(SharedAttributes::HEALTH(), base + addition);
    (void)attrs.setCurrentValue(SharedAttributes::HEALTH(), static_cast<float>(mob.getMaxHealth()));
}

std::string armorId(int material, std::string_view piece) {
    static constexpr std::string_view kMaterials[] = {"netherite", "diamond", "golden", "iron", "leather"};
    return "minecraft:" + std::string(kMaterials[std::clamp(material, 0, 4)]) + "_" + std::string(piece);
}

void equipArmorPiece(Mob& mob, int minDifficulty, core::ArmorRange range, ArmorSlot slot, std::string_view piece, float special) {
    int total = 0;
    for (int m = range.first; m < range.last; ++m) total += core::armorWeight(m);
    if (total <= 0) return;
    int roll     = nextInt(total);
    int material = range.first;
    for (int m = range.first; m < range.last; ++m) {
        if (roll < core::armorWeight(m)) {
            material = m;
            break;
        }
        roll -= core::armorWeight(m);
    }
    auto stack = game::ItemConv::make(armorId(material, piece));
    if (stack.isNull()) return;
    if (nextInt(6 - minDifficulty) == 0) {
        int level = static_cast<int>(5.0f + special * 18.0f + static_cast<float>(minDifficulty * 6));
        EnchantUtils::randomlyEnchant(mob.getLevel().getRandom(), stack, level, 0, false);
    }
    mob.setArmor(slot, stack);
}

// ---- jukebox ---------------------------------------------------------------------------------------------

/// Mobs whose backpack plays (storage created at spawn): unique id -> storage uuid.
struct JukeboxMob {
    std::string uuid;
    int         dim;
    Vec3        pos;
};
std::unordered_map<std::int64_t, JukeboxMob>& jukeboxMobs() {
    static std::unordered_map<std::int64_t, JukeboxMob> m;
    return m;
}

/// addJukeboxUpgradeAndRandomDisc: a jukebox (advanced 1 in 4) in upgrade slot 0; a basic
/// one gets one disc, an advanced one 1..(disc slots / 3) discs into its first slots, each drawn
/// at random (repeats possible) from the music discs not in `discBlockList`.
void addJukeboxUpgradeAndRandomDisc(std::shared_ptr<game::Backpack> const& bp) {
    bool advanced = uniform() < 0.25;
    auto stack    = game::ItemConv::make(advanced ? "alleviatedbackpacks:advanced_jukebox_upgrade" : "alleviatedbackpacks:jukebox_upgrade");
    if (stack.isNull() || bp->data().upgrades.empty()) return;
    auto item = game::ItemConv::toData(stack).withCount(1);
    if (!bp->data().upgrades[0].empty() || !upgrade::install(bp, 0, item, nullptr)) return;
    std::vector<std::string> discs;
    auto const&              blocked = additions().discBlockList;
    for (auto const& id : upgrade::jukebox::vanillaDiscIds()) {
        if (std::find(blocked.begin(), blocked.end(), id) == blocked.end()) discs.push_back(id);
    }
    if (discs.empty()) return; // every disc blocked: the jukebox stays empty
    upgrade::UpgradeContext ctx(bp, 0);
    int numberOfDiscs = advanced ? nextInt(std::max(1, upgrade::jukebox::advancedSlots() / 3)) + 1 : 1;
    for (int slot = 0; slot < numberOfDiscs; ++slot) {
        auto d = game::ItemConv::make(discs[static_cast<std::size_t>(nextInt(static_cast<int>(discs.size())))]);
        if (d.isNull()) continue;
        upgrade::jukebox::setDisc(ctx, slot, game::ItemConv::toData(d).withCount(1));
    }
}

/// Deletes a backpack's storage unless a player has used it (removeContentsUuid: the
/// contents are then unreachable anyway).
void dropStorage(std::string const& uuid) {
    upgrade::jukebox::Service::get().stopAll(uuid);
    if (admin::AccessLog::get().contains(uuid)) return;
    game::Storage::get().erase(uuid);
}

void removeUuid(ItemStack& stack) {
    if (auto uuid = game::backpackUuid(stack)) dropStorage(*uuid);
    setUserData(stack, [](CompoundTag& tag) { tag.remove(game::kUuidKey); });
}

bool isRaidRaider(Mob& mob) {
    auto& dim     = mob.getDimension();
    auto* manager = dim.mVillageManager.get();
    if (!manager) return false;
    auto const& p = mob.getPosition();
    BlockPos    pos{static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y)), static_cast<int>(std::floor(p.z))};
    auto        village = manager->getClosestVillageWithRaid(pos).lock();
    if (!village) return false;
    auto* raid = village->mRaid.get();
    return raid && raid->isRaider(mob.getOrCreateUniqueID());
}

} // namespace

// ---- public API -------------------------------------------------------------------------------------------

std::mt19937& rng() {
    static std::mt19937 engine{std::random_device{}()};
    return engine;
}

bool isMobWorn(ItemStack const& stack) { return hasByte(stack, kMobWornKey); }

std::optional<PendingLoot> pendingLoot(ItemStack const& stack) {
    if (stack.isNull() || !stack.mUserData) return std::nullopt;
    auto const& tag = *stack.mUserData;
    if (!tag.contains(kPendingLootKey, Tag::Type::String)) return std::nullopt;
    std::string const& text = tag[kPendingLootKey];
    auto               j    = nlohmann::json::parse(text, nullptr, false);
    if (!j.is_object()) return std::nullopt;
    PendingLoot loot;
    try {
        loot.table      = j.value("table", std::string{});
        loot.lootFactor = j.contains("factor") ? j.value("factor", 0.0f) : static_cast<float>(j.value("rolls", 0)); // phase-1: rolls
        loot.dimension  = j.value("dim", 0);
        loot.seed       = j.value("seed", 0u);
        loot.upgrades   = j.value("upgrades", false);
        if (j.contains("main")) loot.mainColor = j.value("main", 0u);
        if (j.contains("accent")) loot.accentColor = j.value("accent", 0u);
        if (auto it = j.find("extra"); it != j.end() && it->is_array()) {
            for (auto const& e : *it) {
                auto item = core::ItemStackData::fromJson(e);
                if (!item.empty()) loot.extraItems.push_back(std::move(item));
            }
        }
    } catch (nlohmann::json::exception const&) {
        return std::nullopt;
    }
    return loot;
}

void setPendingLoot(ItemStack& stack, PendingLoot const& loot) {
    nlohmann::json j{
        {"table",    loot.table     },
        {"factor",   loot.lootFactor},
        {"dim",      loot.dimension },
        {"seed",     loot.seed      },
        {"upgrades", loot.upgrades  },
    };
    if (loot.mainColor) j["main"] = *loot.mainColor;
    if (loot.accentColor) j["accent"] = *loot.accentColor;
    if (!loot.extraItems.empty()) {
        auto extra = nlohmann::json::array();
        for (auto const& e : loot.extraItems) extra.push_back(e.toJson());
        j["extra"] = std::move(extra);
    }
    setUserData(stack, [&](CompoundTag& tag) { tag.putString(std::string(kPendingLootKey), j.dump()); });
}

ItemStack makeMobBackpack(core::Tier tier, PendingLoot const& loot) {
    auto stack = game::makeBackpackItem(tier);
    if (stack.isNull()) return stack;
    setPendingLoot(stack, loot);
    setUserData(stack, [&](CompoundTag& tag) { tag.putByte(std::string(kMobWornKey), 1); });
    return stack;
}

void fillBackpack(std::shared_ptr<game::Backpack> const& bp, PendingLoot const& loot) {
    if (!bp) return;
    if (loot.mainColor) bp->data().mainColor = *loot.mainColor;
    if (loot.accentColor) bp->data().accentColor = *loot.accentColor;
    putIntoRandomEmptySlots(*bp, rollLoot(loot), loot.seed);
    for (auto const& extra : loot.extraItems) {
        // fillWithExtraItems: into the inventory (upgrades too, they are items there).
        auto r = bp->inventory().insert(extra, core::AccessContext{core::Source::Internal});
        if (r.inserted < extra.count) log().warn("wearable: extra item {} did not fit into backpack {}", extra.id, bp->uuid());
    }
    if (loot.upgrades) installRandomUpgrades(bp);
    bp->markDirty();
}

bool consumePendingLoot(ItemStack& stack) {
    auto loot = pendingLoot(stack);
    auto uuid = game::backpackUuid(stack);
    if (!loot || !uuid) return false;
    auto bp = game::Storage::get().find(*uuid);
    if (!bp) return false;
    fillBackpack(bp, *loot);
    if (bp->data().settings.is_object()) bp->data().settings.erase(std::string(admin::kPendingLootSetting));
    bp->markDirty();
    setUserData(stack, [](CompoundTag& tag) { tag.remove(kPendingLootKey); });
    return true;
}

std::optional<std::string> lootTableFor(std::string_view typeName) {
    for (auto const& mapping : additions().entityLootTableList) {
        auto bar = mapping.find('|');
        if (bar == std::string::npos) continue;
        if (std::string_view(mapping).substr(0, bar) != typeName) continue;
        auto table = mapping.substr(bar + 1);
        return table == "null" ? std::string{} : table;
    }
    return std::nullopt;
}

bool isEligibleMob(std::string_view typeName) { return lootTableFor(typeName).has_value(); }

std::optional<std::pair<std::uint32_t, std::uint32_t>> spawnEggColors(std::string_view typeName) {
    for (auto const& e : kEggColors) {
        if (e.type == typeName) return std::make_pair(e.background, e.highlight);
    }
    return std::nullopt;
}

float effectiveDifficultyAt(Mob& mob) {
    auto& level      = mob.getLevel();
    int   difficulty = static_cast<int>(level.getDifficulty());
    auto  ticks      = static_cast<std::int64_t>(level.getCurrentTick().tickID);
    int   phase      = static_cast<int>((static_cast<std::int64_t>(level.getTime()) / 24000) % 8);
    // Bedrock keeps no per-chunk inhabited time: the regional part only grows with world age.
    return core::effectiveDifficulty(difficulty, ticks, 0, core::moonBrightness(phase));
}

static std::vector<core::TierChoice> tierPoolAt(int difficultyIndex) {
    // BACKPACK_CHANCES / DIFFICULTY_BACKPACK_CHANCES (fixed weights, not configurable).
    using core::Tier;
    switch (difficultyIndex) {
    case 0:
        return {{Tier::Leather, 625}, {Tier::Copper, 250}, {Tier::Iron, 125}, {Tier::Gold, 25}, {Tier::Diamond, 5}, {Tier::Netherite, 1}};
    case 1:
        return {{Tier::Copper, 250}, {Tier::Iron, 125}, {Tier::Gold, 25}, {Tier::Diamond, 5}};
    default:
        return {{Tier::Copper, 250}, {Tier::Iron, 125}, {Tier::Gold, 25}};
    }
}

core::Tier rollTier(float effectiveDifficulty, std::mt19937& engine) {
    auto pool  = tierPoolAt(core::difficultyIndex(effectiveDifficulty));
    int  total = core::totalWeight(pool);
    if (total <= 0) return core::Tier::Leather;
    return core::pickWeighted(pool, std::uniform_int_distribution<int>(0, total - 1)(engine));
}

bool equipMob(Mob& mob, std::optional<core::Tier> forcedTier, std::optional<bool> forcedJukebox) {
    auto const& c    = additions();
    float       eff  = effectiveDifficultyAt(mob);
    auto        tier = forcedTier.value_or(rollTier(eff, rng()));
    int         minD = core::minDifficultyOf(tier);
    // addBackpack: one difficulty for the loot factor and the potion buffs.
    int  difficulty = std::max(minD, nextInt(kMaxDifficulty + 1));
    auto backpack   = game::makeBackpackItem(tier);
    if (backpack.isNull()) return false;
    setUserData(backpack, [](CompoundTag& tag) { tag.putByte(std::string(kMobWornKey), 1); });
    bool playMusic = forcedJukebox.value_or(c.playJukebox && nextInt(4) == 0);

    // equipBackpack: everything below only for mobs with a spawn egg (colours come from it).
    if (auto egg = spawnEggColors(mob.getTypeName())) {
        PendingLoot loot;
        loot.mainColor    = egg->first | 0xFF000000u;
        loot.accentColor  = egg->second | 0xFF000000u;
        loot.dimension    = static_cast<int>(mob.getDimensionId());
        loot.seed         = static_cast<unsigned>(rng()());
        loot.upgrades     = config::get<WearableConfig>().mobs.addRandomUpgrades;
        auto table        = lootTableFor(mob.getTypeName());
        if (difficulty != 0 && c.addLoot && table && !table->empty()) {
            loot.table      = *table;
            loot.lootFactor = static_cast<float>(difficulty) / static_cast<float>(kMaxDifficulty);
        }
        setPendingLoot(backpack, loot);
        if (playMusic) {
            // The jukebox must play from the mob: the storage is created now with the colours
            // (create listener); the loot still waits for the first open.
            PendingLoot colours;
            colours.mainColor   = loot.mainColor;
            colours.accentColor = loot.accentColor;
            colours.seed        = loot.seed;
            setPendingLoot(backpack, colours);
            setUserData(backpack, [](CompoundTag& tag) { tag.putByte(std::string(kMobJukeboxKey), 1); });
            bool changed = false;
            auto bp      = game::Storage::get().forItem(backpack, &changed);
            if (bp && !bp->data().upgrades.empty()) {
                addJukeboxUpgradeAndRandomDisc(bp);
                if (loot.upgrades) installRandomUpgrades(bp); // AlleviatedBackpacks extra, after the jukebox
                jukeboxMobs()[mob.getOrCreateUniqueID().rawID] = {bp->uuid(), static_cast<int>(mob.getDimensionId()), mob.getPosition()};
            } else {
                setUserData(backpack, [](CompoundTag& tag) { tag.remove(kMobJukeboxKey); });
            }
            PendingLoot rest;
            rest.table      = loot.table;
            rest.lootFactor = loot.lootFactor;
            rest.dimension  = loot.dimension;
            rest.seed       = loot.seed;
            if (!rest.table.empty()) {
                setPendingLoot(backpack, rest);
                // The storage exists before its loot: mark it so load-time cleanup keeps it (the
                // loot table data stays with the backpack until it is first opened).
                if (bp) {
                    bp->data().settings[std::string(admin::kPendingLootSetting)] = true;
                    bp->markDirty();
                }
            }
        }
    }
    // Health before the potions: Bedrock's health boost effect then adds on top of the raised max
    // (in Java the attribute modifier and health boost add up the same way).
    raiseHealth(mob, minD);
    mob.setArmor(ArmorSlot::Torso, backpack);
    applyPotions(mob, difficulty, minD);
    if (c.equipWithArmor) {
        auto  ranges  = core::armorRanges(tier);
        float special = core::specialMultiplier(eff);
        equipArmorPiece(mob, minD, ranges.helmet, ArmorSlot::Head, "helmet", special);
        equipArmorPiece(mob, minD, ranges.leggings, ArmorSlot::Legs, "leggings", special);
        equipArmorPiece(mob, minD, ranges.boots, ArmorSlot::Feet, "boots", special);
    }
    mob.sendArmor(std::bitset<5>{0b01111});
    return true;
}

bool handleMobDeath(Mob& mob, Player* killer) {
    if (mob.isPlayer()) return false;
    auto const& chest = mob.getArmor(ArmorSlot::Torso);
    if (!game::isBackpack(chest)) return false;
    if (hasByte(chest, kMobInertKey)) {
        // Worn since a conversion: equipment with drop chance 0, never dropped.
        mob.setArmor(ArmorSlot::Torso, ItemStack::EMPTY_ITEM());
        return false;
    }
    if (!isMobWorn(chest)) return false;
    ItemStack item = chest;
    mob.setArmor(ArmorSlot::Torso, ItemStack::EMPTY_ITEM());
    jukeboxMobs().erase(mob.getOrCreateUniqueID().rawID);
    auto const& c    = additions();
    bool        drop = killer != nullptr && (c.dropToFakePlayers || !killer->isSimulatedPlayer());
    if (drop) {
        int  looting = EnchantUtils::getEnchantLevel(Enchant::Type::Looting, killer->getSelectedItem());
        auto tier    = game::backpackTierOf(item).value_or(core::Tier::Leather);
        drop         = uniform() < core::dropChance(
                               std::clamp(c.backpackDropChance, 0.0, 1.0),
                               std::clamp(c.lootingChanceIncreasePerLevel, 0.0, 0.3),
                               looting,
                               tier
                           );
    }
    if (!drop) {
        removeUuid(item);
        return false;
    }
    if (hasByte(item, kMobJukeboxKey)) {
        // putJukeboxItemsInContainerAndRemoveStorageUuid: the upgrades (with their discs) leave
        // the storage as extra items of the dropped backpack, which gets fresh storage on open.
        auto loot = pendingLoot(item).value_or(PendingLoot{});
        if (auto uuid = game::backpackUuid(item); uuid) {
            if (auto bp = game::Storage::get().find(*uuid)) {
                upgrade::jukebox::Service::get().stopAll(*uuid);
                for (int slot = 0; slot < static_cast<int>(bp->data().upgrades.size()); ++slot) {
                    if (bp->data().upgrades[static_cast<std::size_t>(slot)].empty()) continue;
                    if (auto removed = upgrade::remove(bp, slot, nullptr)) loot.extraItems.push_back(*removed);
                }
                loot.mainColor   = bp->data().mainColor;
                loot.accentColor = bp->data().accentColor;
            }
        }
        removeUuid(item);
        setPendingLoot(item, loot);
    }
    setUserData(item, [](CompoundTag& tag) {
        tag.remove(kMobWornKey);
        tag.remove(kMobJukeboxKey);
    });
    mob.drop(item, false);
    return true;
}

// ---- conversions and creepers ------------------------------------------------------------------------

static ItemStack leaveLevelWithBackpack(std::int64_t sourceId, ItemStack const& backpack) {
    // removeBackpackUuid (EntityLeaveLevelEvent): the mob that left loses its backpack's
    // storage; its jukebox stops.
    jukeboxMobs().erase(sourceId);
    ItemStack copy = backpack;
    removeUuid(copy);
    // Java's Mob.convertTo copies the equipment (drop chance 0 included) but not the mob's own
    // spawned-with-backpack data: the new mob just wears the item and never drops it.
    setUserData(copy, [](CompoundTag& tag) {
        tag.remove(kMobWornKey);
        tag.remove(kMobJukeboxKey);
        tag.remove(kPendingLootKey);
        tag.putByte(std::string(kMobInertKey), 1);
    });
    return copy;
}

void transferToConversion(std::int64_t sourceId, std::string const& sourceType, Mob& outcome, ItemStack const& backpack) {
    outcome.setArmor(ArmorSlot::Torso, leaveLevelWithBackpack(sourceId, backpack));
    outcome.sendArmor(std::bitset<5>{0b00100});
    log().info("wearable: {} converted into {}; the new mob wears the backpack as plain equipment", sourceType, outcome.getTypeName());
}

namespace {
CreeperStrip gLastStrip;

/// Java's BENEFICIAL category: not harmful and not one of the neutral omens.
bool isBeneficial(uint id) {
    if (id == 0 || id >= static_cast<uint>(MobEffectIds::Count)) return false;
    auto const& type = MobEffect::mMobEffects()[static_cast<MobEffectIds>(id)];
    if (!type || type->mIsHarmful) return false;
    auto e = static_cast<MobEffectIds>(id);
    return e != MobEffectIds::BadOmen && e != MobEffectIds::TrialOmen && e != MobEffectIds::RaidOmen;
}
} // namespace

int stripBeneficialEffects(Mob& creeper) {
    if (!isMobWorn(creeper.getArmor(ArmorSlot::Torso))) return 0; // spawnedWithBackpack tag
    std::vector<uint> remove;
    int               kept = 0;
    for (auto const& e : creeper.getAllEffects()) {
        if (isBeneficial(e.mId)) remove.push_back(e.mId);
        else if (e.mId != 0) ++kept;
    }
    for (auto id : remove) creeper.removeEffect(static_cast<int>(id));
    gLastStrip = {static_cast<int>(remove.size()), kept};
    if (!remove.empty()) log().info("wearable: exploding creeper lost {} beneficial effect(s)", remove.size());
    return static_cast<int>(remove.size());
}

CreeperStrip lastCreeperStrip() { return gLastStrip; }

namespace {

// LL_TYPE_INSTANCE_HOOK declares an unused counter in manual-registration mode.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"

/// Bedrock converts mobs through the transformation component (zombie -> drowned, husk -> zombie,
/// piglin -> zombified piglin, ...). Java's conversion copies the equipment to the new mob and the
/// old one leaves the level (the then removes the backpack's storage). Here the backpack
/// leaves the source first (so `drop_equipment` can't drop it) and the new mob gets it as plain,
/// never-dropped equipment without storage, flags or health bonus.
LL_TYPE_INSTANCE_HOOK(
    AbpMobConversionHook,
    ll::memory::HookPriority::Normal,
    TransformationComponent,
    &TransformationComponent::transformIfAble,
    void,
    ::Actor& actor,
    bool     shouldRemove
) {
    // Only around the moment it converts (cheap while the conversion delay still runs, should the
    // game call this every tick).
    if (mDelayTicks > 1 || actor.isPlayer() || actor.getLevel().isClientSide() || !actor.hasCategory(ActorCategory::Mob)) {
        origin(actor, shouldRemove);
        return;
    }
    ItemStack worn = actor.getArmor(ArmorSlot::Torso);
    if (!game::isBackpack(worn) || !(isMobWorn(worn) || hasByte(worn, kMobInertKey))) {
        origin(actor, shouldRemove);
        return;
    }
    auto&                            source = static_cast<Mob&>(actor);
    auto                             at     = source.getPosition();
    auto                             dim    = source.getDimensionId();
    auto                             self   = source.getOrCreateUniqueID().rawID;
    auto                             type   = source.getTypeName();
    auto&                            level  = source.getLevel();
    std::unordered_set<std::int64_t> before;
    auto& region = source.getDimensionBlockSource();
    AABB  box{Vec3{at.x - 3, at.y - 3, at.z - 3}, Vec3{at.x + 3, at.y + 3, at.z + 3}};
    for (auto* a : region.getEntities(box, 0.f)) {
        if (a) before.insert(a->getOrCreateUniqueID().rawID);
    }
    source.setArmor(ArmorSlot::Torso, ItemStack::EMPTY_ITEM());
    origin(actor, shouldRemove); // `source` may be removed now: only ids from here on
    Mob*  outcome = nullptr;
    float best    = 0.f;
    if (auto d = level.getDimension(dim).lock()) {
        auto& bs = d->getBlockSourceFromMainChunkSource();
        for (auto* a : bs.getEntities(box, 0.f)) {
            if (!a || a->isPlayer() || !a->isAlive() || !a->hasCategory(ActorCategory::Mob)) continue;
            auto id = a->getOrCreateUniqueID().rawID;
            if (id == self || before.contains(id)) continue;
            // The new mob the transformation spawned where the old one stood: the nearest new one.
            auto  p = a->getPosition();
            float d = (p.x - at.x) * (p.x - at.x) + (p.y - at.y) * (p.y - at.y) + (p.z - at.z) * (p.z - at.z);
            if (!outcome || d < best) {
                outcome = static_cast<Mob*>(a);
                best    = d;
            }
        }
    }
    if (outcome) {
        transferToConversion(self, type, *outcome, worn);
        return;
    }
    // No conversion happened (not able yet): the source keeps its backpack.
    auto* still = level.fetchEntity(ActorUniqueID{self}, false);
    if (still && still->isAlive() && still->getArmor(ArmorSlot::Torso).isNull()) {
        still->setArmor(ArmorSlot::Torso, worn);
    } else if (!still || !still->isAlive()) {
        // Gone without a new mob found: it left the level, so its backpack's storage goes too.
        (void)leaveLevelWithBackpack(self, worn);
        log().warn("wearable: {} transformed but no new mob was found for its backpack", type);
    }
}

/// CommonEventHandler.onEntityMobGriefing for creepers (removeBeneficialEffects): the
/// beneficial effects go before the explosion, so the lingering cloud only carries harmful ones.
LL_TYPE_INSTANCE_HOOK(
    AbpCreeperExplodeHook,
    ll::memory::HookPriority::Normal,
    ExplodeComponent,
    &ExplodeComponent::explode,
    void,
    ::Actor&      actor,
    ::Vec3 const& explosionPosition
) {
    if (!actor.getLevel().isClientSide() && actor.getEntityTypeId() == ActorType::Creeper) {
        try {
            (void)stripBeneficialEffects(static_cast<Mob&>(actor));
        } catch (std::exception const& e) {
            log().error("wearable: creeper effect strip failed: {}", e.what());
        }
    }
    origin(actor, explosionPosition);
}

#pragma clang diagnostic pop

} // namespace

namespace {

/// Event wiring, lazy loot generation, jukebox mobs.
class MobBackpacks {
public:
    void install() {
        auto& bus = ll::event::EventBus::getInstance();
        mListeners.push_back(bus.emplaceListener<ll::event::SpawnedMobEvent>([this](ll::event::SpawnedMobEvent& ev) {
            auto mob = ev.mob();
            if (!mob) return;
            Mob& m = *mob;
            if (m.isPlayer() || m.getLevel().isClientSide() || !isEligibleMob(m.getTypeName())) return;
            double chance = std::clamp(additions().chance, 0.0, 1.0);
            if (chance <= 0.0) return;
            int bound = chance >= 1.0 ? 1 : static_cast<int>(1.0 / chance);
            if (nextInt(bound) != 0) return;
            // Next tick: Bedrock registers raid raiders with their raid after the spawn, and the
            // equipment set by the spawn rules is in place by then.
            mPending.push_back(m.getOrCreateUniqueID().rawID);
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::MobDieEvent>([](ll::event::MobDieEvent& ev) {
            auto&   mob    = ev.self();
            Player* killer = nullptr;
            auto    id     = ev.source().getDamagingEntityUniqueID();
            if (id.rawID == -1) id = ev.source().getEntityUniqueID();
            if (auto* a = mob.getLevel().fetchEntity(id, false); a && a->isPlayer()) killer = static_cast<Player*>(a);
            try {
                handleMobDeath(mob, killer);
            } catch (std::exception const& e) {
                log().error("wearable: mob death handling failed: {}", e.what());
            }
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([this](ll::event::ServerLevelTickEvent& ev) {
            tick(ev.level());
        }));
        mCreateListener = game::Storage::get().addCreateListener([](ItemStack& stack, std::shared_ptr<game::Backpack> const& bp) {
            auto loot = pendingLoot(stack);
            if (!loot) return;
            try {
                fillBackpack(bp, *loot);
            } catch (std::exception const& e) {
                // The backpack keeps what was generated so far; the uuid is written anyway.
                log().error("wearable: generating loot for backpack {} failed: {}", bp->uuid(), e.what());
            }
            setUserData(stack, [&](CompoundTag& tag) {
                tag.remove(kPendingLootKey);
                if (!stack.mUserData || !stack.mUserData->contains(kMobJukeboxKey)) tag.remove(kMobWornKey);
            });
        });
        ui::MenuManager::Observer o;
        o.beforeShow = [](Player& player, ui::Menu& menu, int) {
            auto* bm = dynamic_cast<ui::BackpackMenu*>(&menu);
            if (!bm || !bm->source()) return;
            ItemStack stack = game::itemAt(player, *bm->source());
            if (consumePendingLoot(stack)) game::setItemAt(player, *bm->source(), stack);
        };
        mObserver = ui::MenuManager::get().addObserver(std::move(o));
        AbpMobConversionHook::hook();
        AbpCreeperExplodeHook::hook();
    }
    void uninstall() {
        AbpCreeperExplodeHook::unhook();
        AbpMobConversionHook::unhook();
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        game::Storage::get().removeCreateListener(mCreateListener);
        ui::MenuManager::get().removeObserver(mObserver);
        mPending.clear();
        jukeboxMobs().clear();
    }

private:
    void tick(Level& level) {
        ++mTicks;
        if (!mPending.empty()) {
            auto pending = std::move(mPending);
            mPending.clear();
            for (auto id : pending) {
                ActorUniqueID uid;
                uid.rawID = id;
                auto* actor = level.fetchEntity(uid, false);
                if (!actor || !actor->isAlive() || actor->isPlayer()) continue;
                auto& mob = static_cast<Mob&>(*actor);
                if (!mob.getArmor(ArmorSlot::Torso).isNull() || isRaidRaider(mob)) continue;
                try {
                    equipMob(mob);
                } catch (std::exception const& e) {
                    log().error("wearable: equipping {} failed: {}", mob.getTypeName(), e.what());
                }
            }
        }
        if (mTicks % 10 == 0) tickJukeboxMobs(level);
    }

    /// Jukebox mobs play attached to themselves (onLivingUpdate + play(livingEntity)); a
    /// mob that left the level loses its backpack's storage (removeBackpackUuid).
    static void tickJukeboxMobs(Level& level) {
        auto& mobs = jukeboxMobs();
        for (auto it = mobs.begin(); it != mobs.end();) {
            ActorUniqueID uid;
            uid.rawID   = it->first;
            auto* actor = level.fetchEntity(uid, false);
            auto  uuid  = it->second.uuid;
            if (actor && actor->isAlive() && game::backpackUuid(actor->getArmor(ArmorSlot::Torso)) == uuid) {
                it->second.pos = actor->getPosition();
                it->second.dim = static_cast<int>(actor->getDimensionId());
                if (auto bp = game::Storage::get().find(uuid)) {
                    auto& jukebox = upgrade::jukebox::Service::get();
                    // wrapper.play(livingEntity): the music is attached to the mob.
                    jukebox.tickCarrier(bp, it->first);
                    if (!jukebox.playing(uuid, 0)) (void)jukebox.play(upgrade::UpgradeContext(bp, 0), nullptr);
                }
                ++it;
                continue;
            }
            dropStorage(uuid);
            it = mobs.erase(it);
        }
    }

    std::vector<ll::event::ListenerPtr> mListeners;
    std::vector<std::int64_t>           mPending;
    int                                 mCreateListener = 0;
    int                                 mObserver       = 0;
    std::int64_t                        mTicks          = 0;
};

} // namespace
} // namespace abp::wearable

ABP_REGISTER_MODULE(abp::wearable::MobBackpacks);
