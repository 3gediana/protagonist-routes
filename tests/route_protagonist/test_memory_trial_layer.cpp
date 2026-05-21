#include "core/agent/Agent.h"
#include "brain/ProtagonistBrain.h"
#include "core/brain/fixed/FixedTopologyBrain.h"
#include "core/brain/fixed/FixedTopologyGenome.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "world/MemoryTrialLayer.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>

namespace {

using neuro::AgentId;
using neuro::FixedTopologyBrain;
using neuro::FixedTopologyGenome;
using neuro::GenomeId;
using neuro::routes::protagonist::ProtagonistBrain;
using neuro::Vec2;
using neuro::routes::protagonist::MemoryTrialConfig;
using neuro::routes::protagonist::MemoryTrialLayer;

std::unique_ptr<neuro::Agent> makeStubProtagonistAgent(AgentId id, Vec2 pos) {
    auto brain = std::make_unique<ProtagonistBrain>(
        1,
        2,
        2,
        2,
        1,
        1,
        static_cast<std::uint32_t>(id.value + 17),
        false);
    return std::make_unique<neuro::Agent>(
        id,
        GenomeId{static_cast<std::uint64_t>(id.value)},
        std::move(brain),
        pos,
        1000.0f);
}

std::unique_ptr<neuro::Agent> makeStubBackgroundAgent(AgentId id, Vec2 pos) {
    FixedTopologyGenome genome{GenomeId{static_cast<std::uint64_t>(id.value)}, 4, 4, 4};
    auto brain = std::make_unique<FixedTopologyBrain>(genome);
    return std::make_unique<neuro::Agent>(
        id,
        GenomeId{static_cast<std::uint64_t>(id.value)},
        std::move(brain),
        pos,
        1000.0f);
}

}  // namespace

TEST(MemoryTrialLayer, ExposesCueAndRewardsCorrectTarget) {
    using namespace neuro;

    World2D world(WorldId{1}, Vec2{120.0f, 120.0f});
    auto agents_uniq = std::make_unique<AgentLayer>(-1.0e9f, 0.0f);
    auto* agents = agents_uniq.get();
    world.addLayer(std::move(agents_uniq));

    MemoryTrialConfig config;
    config.enabled = true;
    config.cue_duration_seconds = 1.0;
    config.choice_deadline_seconds = 10.0;
    auto trial_uniq = std::make_unique<MemoryTrialLayer>(config, 7u);
    auto* trial = trial_uniq.get();
    world.addLayer(std::move(trial_uniq));

    const auto cue = trial->cueFor(trial->cueAnchor());
    EXPECT_FLOAT_EQ(cue[trial->correctChannel()], 1.0f);
    EXPECT_FLOAT_EQ(cue[1u - trial->correctChannel()], 0.0f);

    world.tick(1.2f);

    auto agent = makeStubProtagonistAgent(AgentId{1}, Vec2{0.0f, 0.0f});
    auto* agent_ptr = agent.get();
    agents->addAgent(std::move(agent), world);

    trial->tick(world, 0.1f);
    EXPECT_LT(Vec2::distanceSquared(agent_ptr->position(), trial->hubAnchor()), 4.0f);

    agent_ptr->setPosition(trial->correctChannel() == 0 ? trial->leftTarget() : trial->rightTarget());
    trial->tick(world, 0.1f);

    EXPECT_GT(agent_ptr->fitness(), 0.0);
    EXPECT_EQ(trial->successCount(), 1u);
    EXPECT_EQ(trial->failureCount(), 0u);
}

TEST(MemoryTrialLayer, PenalizesWrongTarget) {
    using namespace neuro;

    World2D world(WorldId{2}, Vec2{120.0f, 120.0f});
    auto agents_uniq = std::make_unique<AgentLayer>(-1.0e9f, 0.0f);
    auto* agents = agents_uniq.get();
    world.addLayer(std::move(agents_uniq));

    MemoryTrialConfig config;
    config.enabled = true;
    config.cue_duration_seconds = 1.0;
    config.choice_deadline_seconds = 10.0;
    auto trial_uniq = std::make_unique<MemoryTrialLayer>(config, 11u);
    auto* trial = trial_uniq.get();
    world.addLayer(std::move(trial_uniq));

    world.tick(1.2f);

    auto agent = makeStubProtagonistAgent(AgentId{1}, Vec2{0.0f, 0.0f});
    auto* agent_ptr = agent.get();
    agents->addAgent(std::move(agent), world);

    trial->tick(world, 0.1f);
    agent_ptr->setPosition(trial->correctChannel() == 0 ? trial->rightTarget() : trial->leftTarget());
    trial->tick(world, 0.1f);

    EXPECT_LT(agent_ptr->fitness(), 0.0);
    EXPECT_EQ(trial->successCount(), 0u);
    EXPECT_EQ(trial->failureCount(), 1u);
}

TEST(MemoryTrialLayer, IgnoresNonProtagonistAgents) {
    using namespace neuro;

    World2D world(WorldId{3}, Vec2{120.0f, 120.0f});
    auto agents_uniq = std::make_unique<AgentLayer>(-1.0e9f, 0.0f);
    auto* agents = agents_uniq.get();
    world.addLayer(std::move(agents_uniq));

    MemoryTrialConfig config;
    config.enabled = true;
    config.cue_duration_seconds = 1.0;
    config.choice_deadline_seconds = 10.0;
    auto trial_uniq = std::make_unique<MemoryTrialLayer>(config, 13u);
    auto* trial = trial_uniq.get();
    world.addLayer(std::move(trial_uniq));

    world.tick(1.2f);

    auto agent = makeStubBackgroundAgent(AgentId{1}, Vec2{0.0f, 0.0f});
    auto* agent_ptr = agent.get();
    agents->addAgent(std::move(agent), world);

    trial->tick(world, 0.1f);
    agent_ptr->setPosition(trial->correctChannel() == 0 ? trial->leftTarget() : trial->rightTarget());
    trial->tick(world, 0.1f);

    EXPECT_DOUBLE_EQ(agent_ptr->fitness(), 0.0);
    EXPECT_EQ(trial->successCount(), 0u);
    EXPECT_EQ(trial->failureCount(), 0u);
}
