// Smoke coverage for NoveltyArchive.
//
// The class has been around since F-NOVELTY but never had a direct test.
// Stage 5 added MapElitesArchive next to it; both classes share the same
// BehaviorCharacterization, so it's worth pinning the older novelty
// behavior so future refactors can't silently break NoveltyReward.

#include "core/evolution/novelty/NoveltyArchive.h"

#include <gtest/gtest.h>

namespace {
using neuro::BehaviorCharacterization;
using neuro::NoveltyArchive;
}

TEST(NoveltyArchive, EmptyArchiveReturnsMaximumNovelty) {
    NoveltyArchive arch(/*k_neighbors*/ 5, /*threshold*/ 0.3f);
    BehaviorCharacterization bc{0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    EXPECT_FLOAT_EQ(arch.novelty(bc), 1.0f);
    EXPECT_EQ(arch.size(), 0u);
}

TEST(NoveltyArchive, KnownPointHasZeroSelfDistance) {
    NoveltyArchive arch(/*k_neighbors*/ 1, /*threshold*/ 0.0f);
    BehaviorCharacterization a{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    arch.add(a);
    EXPECT_EQ(arch.size(), 1u);
    // Distance from a to a is 0 -> averaged k=1 nearest neighbour distance.
    EXPECT_NEAR(arch.novelty(a), 0.0f, 1e-5f);
}

TEST(NoveltyArchive, NewPointFurtherFromArchiveIsMoreNovel) {
    NoveltyArchive arch(/*k_neighbors*/ 2, /*threshold*/ 0.0f);
    arch.add({0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    arch.add({0.1f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});

    BehaviorCharacterization near{0.05f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    BehaviorCharacterization far {5.0f,  5.0f,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    EXPECT_GT(arch.novelty(far), arch.novelty(near));
}

TEST(NoveltyArchive, ClearResetsState) {
    NoveltyArchive arch(/*k_neighbors*/ 1, /*threshold*/ 0.0f);
    arch.add({1, 2, 3, 4, 5, 6, 7, 8});
    arch.add({9, 8, 7, 6, 5, 4, 3, 2});
    EXPECT_EQ(arch.size(), 2u);
    arch.clear();
    EXPECT_EQ(arch.size(), 0u);
    BehaviorCharacterization bc{0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_FLOAT_EQ(arch.novelty(bc), 1.0f);
}
