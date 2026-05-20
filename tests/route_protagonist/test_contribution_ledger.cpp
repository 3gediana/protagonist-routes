// Stage 2 - P3 ContributionLedger acceptance test.
//
// Verifies the WorksiteLayer BuildCompleted payload carries time-weighted
// contribution data, with the dominant builder ranked first and a fly-by
// "passer-by" agent suppressed below the runtime's free-rider threshold.
//
// We bypass the full protagonist runtime here: a small World2D wired with
// BaseLayer + AgentLayer + ObjectLayer + WorksiteLayer is enough to drive
// progressConstruction.

#include "core/agent/Agent.h"
#include "core/brain/fixed/FixedTopologyBrain.h"
#include "core/brain/fixed/FixedTopologyGenome.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/BaseLayer.h"
#include "world/WorksiteLayer.h"

#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

namespace {

using neuro::AgentId;
using neuro::FixedTopologyBrain;
using neuro::FixedTopologyGenome;
using neuro::GenomeId;
using neuro::Vec2;
using neuro::events::BuildCompleted;
using neuro::events::Event;
using neuro::events::EventType;
using neuro::routes::protagonist::BaseLayer;
using neuro::routes::protagonist::WorksiteLayer;

// A small fixed-topology brain. We never run inference - the brain is just
// here to satisfy Agent's invariants.
std::unique_ptr<neuro::Agent> makeStubAgent(AgentId id, Vec2 pos) {
    FixedTopologyGenome g{GenomeId{static_cast<std::uint64_t>(id.value)}, 4, 4, 4};
    auto brain = std::make_unique<FixedTopologyBrain>(g);
    return std::make_unique<neuro::Agent>(
        id,
        GenomeId{static_cast<std::uint64_t>(id.value)},
        std::move(brain),
        pos,
        /*energy=*/1000.0f);
}

}  // namespace

TEST(ContributionLedger, BuildCompletedRanksByTimeAndDropsPassersBy) {
    using namespace neuro;
    using namespace neuro::routes::protagonist;

    World2D world(WorldId{1}, Vec2{50.0f, 50.0f});

    // BaseLayer keeps WorksiteLayer's spawnSites() happy (it looks up the
    // base center + stockpile direction to lay out worksites).
    world.addLayer(std::make_unique<BaseLayer>(
        /*center*/        Vec2{15.0f, 25.0f},
        /*base_radius*/   6.0f,
        /*stockpile_radius*/ 4.0f,
        /*nest_radius*/   3.0f,
        /*stockpile_offset*/ 12.0f,
        /*nest_offset*/   -8.0f));

    // AgentLayer with a near-infinite-negative death threshold so our stub
    // agents do not die from metabolism during the test.
    auto agent_layer_uniq = std::make_unique<AgentLayer>(/*death*/ -1.0e9f, /*metabolism*/ 0.0f);
    auto* agents = agent_layer_uniq.get();
    world.addLayer(std::move(agent_layer_uniq));

    world.addLayer(std::make_unique<ObjectLayer>(Vec2{50.0f, 50.0f}, /*object_count*/ 0));

    auto worksite_uniq = std::make_unique<WorksiteLayer>(
        /*site_count*/ 1,
        /*spawn_radius*/ 6.0f,
        /*interaction_radius*/ 5.0f,
        /*required_sticks*/ 0,
        /*required_stones*/ 0,
        /*assembly_rate*/ 0.10f,
        /*shelter_energy_bonus*/ 0.0f,
        /*seed*/ 1u);
    auto* worksites = worksite_uniq.get();
    world.addLayer(std::move(worksite_uniq));

    ASSERT_GE(worksites->sites().size(), 1u);
    ASSERT_TRUE(worksites->activateNextDormant());
    const Vec2 site_pos = worksites->sites().front().position;
    const float site_radius = worksites->sites().front().radius;
    const float inside_dx = site_radius * 0.5f;

    auto builder_long  = makeStubAgent(AgentId{1}, site_pos + Vec2{inside_dx, 0.0f});
    auto builder_short = makeStubAgent(AgentId{2}, site_pos + Vec2{-inside_dx, 0.0f});
    auto passerby      = makeStubAgent(AgentId{3}, site_pos + Vec2{0.0f, 50.0f});
    auto* builder_long_p  = builder_long.get();
    auto* builder_short_p = builder_short.get();
    auto* passerby_p      = passerby.get();
    agents->addAgent(std::move(builder_long), world);
    agents->addAgent(std::move(builder_short), world);
    agents->addAgent(std::move(passerby), world);

    BuildCompleted observed{};
    bool observed_set = false;
    world.eventBus().subscribe(EventType::BuildCompleted, [&](const Event& evt) {
        observed = evt.payloadAs<BuildCompleted>();
        observed_set = true;
    });

    // Drive WorksiteLayer directly (bypass world.tick to avoid AgentLayer
    // running Agent::decide on our stub agents - they have no perceptions
    // and would throw on input dim mismatch).
    constexpr float kDt = 0.5f;
    constexpr int   kTotalSteps = 60;
    constexpr int   kPasserbySteps = 1;
    constexpr int   kShortSteps = 6;

    for (int step = 0; step < kTotalSteps; ++step) {
        builder_long_p->setPosition(site_pos + Vec2{inside_dx, 0.0f});

        builder_short_p->setPosition(step < kShortSteps
            ? site_pos + Vec2{-inside_dx, 0.0f}
            : site_pos + Vec2{0.0f, 40.0f});

        passerby_p->setPosition(step < kPasserbySteps
            ? site_pos + Vec2{0.0f, inside_dx}
            : site_pos + Vec2{0.0f, 50.0f});

        worksites->tick(world, kDt);
        world.eventBus().flush();
        if (observed_set) break;
    }

    ASSERT_TRUE(observed_set) << "BuildCompleted never fired";
    ASSERT_EQ(observed.contributors.size(), observed.contribution_seconds.size());
    ASSERT_FALSE(observed.contributors.empty());

    // Long builder must be top.
    EXPECT_EQ(observed.contributors.front().value, AgentId{1}.value);
    EXPECT_GT(observed.contribution_seconds.front(), 5.0f);

    auto find_seconds = [&](AgentId id) -> float {
        for (std::size_t i = 0; i < observed.contributors.size(); ++i) {
            if (observed.contributors[i] == id) return observed.contribution_seconds[i];
        }
        return -1.0f;
    };

    const float short_secs = find_seconds(AgentId{2});
    EXPECT_GT(short_secs, 0.0f);
    EXPECT_LT(short_secs, observed.contribution_seconds.front());

    // Passer-by: either absent from the ledger, or below the runtime's
    // 0.5s free-rider threshold (their effective ledger share is ~0).
    const float passer_secs = find_seconds(AgentId{3});
    if (passer_secs >= 0.0f) {
        EXPECT_LT(passer_secs, 0.6f);
    }
}
