// Void upgrade insert rules: the two modes of VoidUpgradeWrapper ("Void Any" and
// "Void Overflow", `shouldVoidOverflow`) on top of the insert path.

#include <gtest/gtest.h>

#include "upgrade/void/VoidRules.h"

using namespace abp::core;
using namespace abp::upgrade::voiding;

namespace {
ItemStackData cobble(int n) { return ItemStackData::of("minecraft:cobblestone", n); }
ItemStackData dirt(int n) { return ItemStackData::of("minecraft:dirt", n); }
AccessContext const kPickup{Source::Pickup};
AccessContext const kPlayer{Source::Player};

VoidRules rules(bool overflowOnly, bool gui = false, bool exact = false) {
    VoidRules r;
    r.overflowOnly    = overflowOnly;
    r.workInGui       = gui;
    r.matchNbt        = exact;
    r.matchDurability = exact;
    r.matches         = [](ItemStackData const& i) { return i.id == "minecraft:cobblestone"; };
    return r;
}

void install(BackpackInventory& inv, VoidRules r) {
    inv.addInsertFilter("void", 1001, [r](InsertQuery const& q) { return verdict(r, q); });
}
} // namespace

TEST(VoidRules, VoidAnyVoidsAutomaticInsertsOnly) {
    BackpackInventory inv(3);
    install(inv, rules(false));
    auto r = inv.insert(cobble(10), kPickup);
    EXPECT_EQ(r.voided, 10);
    r = inv.insert(dirt(10), kPickup);
    EXPECT_EQ(r.inserted, 10);
    r = inv.insert(cobble(10), kPlayer);
    EXPECT_EQ(r.inserted, 10); // GUI: voided on the next tick by the upgrade, not here
    // No room and none stored: rejected before any upgrade sees it.
    BackpackInventory full(1);
    full.set(0, dirt(64));
    install(full, rules(false));
    r = full.insert(cobble(5), kPickup);
    EXPECT_EQ(r.voided, 0);
    EXPECT_EQ(r.remainder.count, 5);
}

TEST(VoidRules, OverflowKeepsOneStack) {
    BackpackInventory inv(4);
    install(inv, rules(true));
    auto r = inv.insert(cobble(100), kPickup);
    EXPECT_EQ(r.inserted, 64);
    EXPECT_EQ(r.voided, 36);
    r = inv.insert(cobble(1), kPickup);
    EXPECT_EQ(r.voided, 1);
    // Direct insert into another empty slot is voided too.
    r = inv.insertInto(2, cobble(3), kPickup);
    EXPECT_EQ(r.voided, 3);
    EXPECT_TRUE(inv.at(2).empty());
}

TEST(VoidRules, OverflowTopsUpPartialStack) {
    BackpackInventory inv(4);
    inv.set(1, cobble(30));
    install(inv, rules(true));
    auto r = inv.insert(cobble(50), kPickup);
    EXPECT_EQ(r.inserted, 34);
    EXPECT_EQ(r.voided, 16);
    EXPECT_EQ(inv.at(1).count, 64);
}

TEST(VoidRules, OverflowWithExactMatchingStartsStacksUntilOneIsFull) {
    BackpackInventory inv(4);
    install(inv, rules(true, false, true));
    auto r = inv.insert(cobble(100), kPickup);
    EXPECT_EQ(r.inserted, 100);
    EXPECT_EQ(r.voided, 0);
    r = inv.insert(cobble(10), kPickup);
    EXPECT_EQ(r.voided, 10);
}

TEST(VoidRules, GuiOverflowOnlyWithWorkInGui) {
    for (bool overflowOnly : {true, false}) {
        BackpackInventory inv(3);
        install(inv, rules(overflowOnly, true));
        auto r = inv.insertInto(0, cobble(60), kPlayer);
        EXPECT_EQ(r.inserted, 60);
        r = inv.insertInto(0, cobble(10), kPlayer);
        EXPECT_EQ(r.inserted, 4);
        EXPECT_EQ(r.voided, 6);
        r = inv.insertInto(1, cobble(5), kPlayer);
        EXPECT_EQ(r.voided, 5);
        // Shift-click (automatic placement) merges and voids the rest.
        r = inv.insert(cobble(5), kPlayer);
        EXPECT_EQ(r.voided, 5);
    }

    BackpackInventory plain(3);
    install(plain, rules(true, false));
    plain.insertInto(0, cobble(60), kPlayer);
    auto r = plain.insertInto(1, cobble(5), kPlayer);
    EXPECT_EQ(r.inserted, 5);
}

TEST(VoidRules, InternalNeverVoided) {
    BackpackInventory inv(2);
    install(inv, rules(false));
    auto r = inv.insert(cobble(5), AccessContext{Source::Internal});
    EXPECT_EQ(r.inserted, 5);
}
