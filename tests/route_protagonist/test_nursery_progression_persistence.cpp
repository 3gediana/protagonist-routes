// Stone Bootstrap commit 1 tests for cross-episode progression
// persistence. The NurseryProgressionLayer used to reset all unlock
// flags to false on every episode boundary; the new
// seedFromProgressionState / progressionState pair lets the route
// runtime carry the unlock state forward so the protagonist doesn't
// have to re-earn "stone unlock" from zero every single episode.

#include "world/NurseryProgressionLayer.h"

#include <gtest/gtest.h>

namespace {

using neuro::routes::protagonist::ArchetypeProgressionState;
using neuro::routes::protagonist::NurseryProgressionLayer;

NurseryProgressionLayer makeLayer() {
    // Numeric constants here are not exercised; the persistence API
    // only touches unlock booleans, so any sensible defaults work.
    return NurseryProgressionLayer(
        /*first_fire_stick_threshold=*/3,
        /*worksite_unlock_deposit_threshold=*/4,
        /*stone_unlock_deposit_threshold=*/6,
        /*heavy_unlock_deposit_threshold=*/12,
        /*second_fire_deposit_threshold=*/10,
        /*worksite_unlock_stone_threshold=*/2,
        /*heavy_unlock_stone_threshold=*/4,
        /*second_fire_stone_threshold=*/6,
        /*stone_unlock_count=*/10,
        /*heavy_unlock_count=*/4,
        /*unlock_radius_min=*/3.0f,
        /*unlock_radius_max=*/9.0f,
        /*event_min_interval_seconds=*/0.5,
        /*event_max_interval_seconds=*/2.0,
        /*seed=*/42);
}

}  // namespace

// A freshly-constructed layer must have every unlock flag = false so
// the route runtime treats a "never persisted before" archetype like
// the legacy episode-reset path.
TEST(NurseryProgressionPersistence, DefaultStateIsAllFalse) {
    auto layer = makeLayer();
    const auto s = layer.progressionState();
    EXPECT_FALSE(s.first_fire_ignited);
    EXPECT_FALSE(s.worksite_unlocked);
    EXPECT_FALSE(s.stone_unlocked);
    EXPECT_FALSE(s.heavy_unlocked);
    EXPECT_FALSE(s.second_fire_ignited);
}

// seedFromProgressionState must round-trip through progressionState():
// every true field comes back true; every false field stays false.
TEST(NurseryProgressionPersistence, SeedRoundtripsThroughProgressionState) {
    auto layer = makeLayer();
    ArchetypeProgressionState in{};
    in.first_fire_ignited  = true;
    in.worksite_unlocked   = false;
    in.stone_unlocked      = true;
    in.heavy_unlocked      = false;
    in.second_fire_ignited = true;
    layer.seedFromProgressionState(in);

    const auto out = layer.progressionState();
    EXPECT_TRUE (out.first_fire_ignited);
    EXPECT_FALSE(out.worksite_unlocked);
    EXPECT_TRUE (out.stone_unlocked);
    EXPECT_FALSE(out.heavy_unlocked);
    EXPECT_TRUE (out.second_fire_ignited);
}

// OR-merge semantics: a stale-false seed must NEVER clear a flag that
// is already true. This protects against an out-of-order persistence
// write that lost some bits but kept others.
TEST(NurseryProgressionPersistence, SeedIsMonotonicOrMerge) {
    auto layer = makeLayer();
    // First seed: lock in stone_unlocked + first_fire_ignited.
    layer.seedFromProgressionState({
        /*first_fire_ignited=*/true,
        /*worksite_unlocked=*/false,
        /*stone_unlocked=*/true,
        /*heavy_unlocked=*/false,
        /*second_fire_ignited=*/false,
    });
    // Second seed: stale-false on stone but true on heavy. Stone must
    // stay true; heavy must flip true.
    layer.seedFromProgressionState({
        /*first_fire_ignited=*/false,
        /*worksite_unlocked=*/true,
        /*stone_unlocked=*/false,
        /*heavy_unlocked=*/true,
        /*second_fire_ignited=*/false,
    });

    const auto out = layer.progressionState();
    EXPECT_TRUE(out.first_fire_ignited);   // kept from seed 1
    EXPECT_TRUE(out.worksite_unlocked);    // gained from seed 2
    EXPECT_TRUE(out.stone_unlocked);       // stayed true despite stale-false in seed 2
    EXPECT_TRUE(out.heavy_unlocked);       // gained from seed 2
    EXPECT_FALSE(out.second_fire_ignited); // never set in either
}
