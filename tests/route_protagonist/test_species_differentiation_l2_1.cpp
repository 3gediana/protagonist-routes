// L2.1 species differentiation v2 (SPECIES_DIFFERENTIATION_SPEC.md):
//   - prey-side damage_threshold accumulator (wolfpack emergence)
//   - leopard stealth state + perception line-of-sight filter
//   - small_forager cave hide + perception filter
//   - apex digest_satiety triggered by threshold-class kill
//   - scavenger scavenge_efficiency on low-vitality prey
// These tests lock the contract: do mechanism changes really make
// "single wolf can't kill a deer" true, and does stealth filter
// really hide a static leopard from prey 4m away?
#include "brain/ProtagonistBrain.h"
#include "capability/CaveHideCapability.h"
#include "capability/StealthCapability.h"
#include "core/agent/Agent.h"
#include "core/brain/fixed/FixedTopologyBrain.h"
#include "core/brain/fixed/FixedTopologyGenome.h"
#include "core/capability/PredatorHuntCapability.h"
#include "core/interfaces/IAgentCapability.h"
#include "core/perception/SelfPerception.h"
#include "core/types/Vec2.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "perception/AgentSocialPerception.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <random>

namespace {

using neuro::Agent;
using neuro::AgentId;
using neuro::AgentLayer;
using neuro::FixedTopologyBrain;
using neuro::FixedTopologyGenome;
using neuro::GenomeId;
using neuro::PredatorHuntCapability;
using neuro::SelfPerception;
using neuro::SimTimeSeconds;
using neuro::Vec2;
using neuro::World2D;
using neuro::WorldId;
using neuro::routes::protagonist::AgentSocialPerception;
using neuro::routes::protagonist::CaveHideCapability;
using neuro::routes::protagonist::ProtagonistBrain;
using neuro::routes::protagonist::StealthCapability;

// Build a minimal Agent with a ProtagonistBrain (we never actually
// drive the brain; we just need an Agent we can give capabilities to
// and call apply on directly, bypassing the decide loop).
std::unique_ptr<Agent> makeAgent(AgentId id, Vec2 pos, float energy = 100.0f) {
    auto brain = std::make_unique<ProtagonistBrain>(
        8, 8, 4, 8, 2, 3, static_cast<std::uint32_t>(id.value + 7), false);
    auto agent = std::make_unique<Agent>(
        id,
        GenomeId{static_cast<std::uint64_t>(id.value)},
        std::move(brain),
        pos,
        energy);
    return agent;
}

// No-op capability that absorbs the brain's outputs without doing
// anything. Avoids "Agent capability outputs do not fully consume
// brain outputs" when world.tick runs the test agents in tests that
// drive capabilities (PredatorHuntCapability) externally.
class NoOpAbsorber final : public neuro::IAgentCapability {
public:
    explicit NoOpAbsorber(std::size_t consumed) : consumed_(consumed) {}
    std::size_t consumedOutputs() const override { return consumed_; }
    void apply(neuro::IAgent&,
               neuro::IWorld&,
               std::span<const float>,
               float) override {}
    const char* name() const override { return "noop_absorber"; }

private:
    std::size_t consumed_;
};

// Tickable variant: a tiny FixedTopologyBrain (input=2, output=1) with
// matching SelfPerception + a NoOpAbsorber that consumes the single
// output. Used in tests that need world->tick(dt) to advance time.
// The brain's output is intentionally unused; we drive PredatorHunt
// directly with mock outputs.
std::unique_ptr<Agent> makeTickableAgent(AgentId id, Vec2 pos, float energy = 100.0f) {
    std::mt19937 rng(static_cast<std::uint32_t>(id.value + 17));
    auto genome = FixedTopologyGenome::random(
        GenomeId{id.value},
        /*input_dim=*/2, /*hidden_dim=*/2, /*output_dim=*/1, rng);
    auto brain = std::make_unique<FixedTopologyBrain>(*genome);
    auto agent = std::make_unique<Agent>(
        id,
        GenomeId{static_cast<std::uint64_t>(id.value)},
        std::move(brain),
        pos,
        energy);
    agent->addPerception(std::make_unique<SelfPerception>(200.0f, 5000.0));
    agent->addCapability(std::make_unique<NoOpAbsorber>(/*consumed=*/1));
    return agent;
}

std::unique_ptr<World2D> makeWorld(AgentLayer*& agents_out) {
    auto world = std::make_unique<World2D>(WorldId{1}, Vec2{60.0f, 60.0f});
    auto agent_layer = std::make_unique<AgentLayer>(-1.0e9f, 0.0f, 1);
    agents_out = agent_layer.get();
    world->addLayer(std::move(agent_layer));
    return world;
}

}  // namespace

// ---- Threat damage window primitives ----

TEST(SpeciesDiffL21_ThreatWindow, AccumulatesAndDecays) {
    auto a = makeAgent(AgentId{1}, Vec2{0, 0});
    a->accumulateThreatDamage(AgentId{10}, 3.0f, 0.0);
    a->accumulateThreatDamage(AgentId{11}, 4.0f, 0.5);
    EXPECT_NEAR(a->threatDamageInWindow(0.5, 1.0), 7.0f, 1e-5);
    // After 1.5s, the 0.0s entry is outside window=1.0 from 1.5s
    EXPECT_NEAR(a->threatDamageInWindow(1.5, 1.0), 4.0f, 1e-5);
    // After 2.0s, both are out
    EXPECT_NEAR(a->threatDamageInWindow(2.0, 1.0), 0.0f, 1e-5);
}

TEST(SpeciesDiffL21_ThreatWindow, ContributorCountIsUnique) {
    auto a = makeAgent(AgentId{1}, Vec2{0, 0});
    a->accumulateThreatDamage(AgentId{10}, 1.0f, 0.0);
    a->accumulateThreatDamage(AgentId{10}, 1.0f, 0.1);  // same attacker
    a->accumulateThreatDamage(AgentId{11}, 1.0f, 0.2);
    a->accumulateThreatDamage(AgentId{12}, 1.0f, 0.3);
    EXPECT_EQ(a->threatDamageContributorCount(0.3, 1.0), std::size_t{3});
}

TEST(SpeciesDiffL21_ThreatWindow, ClearWipesAllHistory) {
    auto a = makeAgent(AgentId{1}, Vec2{0, 0});
    a->accumulateThreatDamage(AgentId{10}, 5.0f, 0.0);
    a->clearThreatDamageWindow();
    EXPECT_EQ(a->threatDamageHistory().size(), std::size_t{0});
    EXPECT_FLOAT_EQ(a->threatDamageInWindow(0.0, 1.0), 0.0f);
}

// ---- Damage threshold via PredatorHuntCapability ----

namespace {
// Configure a wolf-flavoured predator using PredatorHuntCapability with
// damage 4 (the L2.1 v2 nerf). A single wolf cannot break LargeHerbivore
// threshold 12 in the 1s window: 4 dmg/sec * 1s = 4 < 12.
std::unique_ptr<Agent> makeWolf(AgentId id, Vec2 pos) {
    auto a = makeTickableAgent(id, pos, /*energy=*/200.0f);
    a->setPredator(true);
    return a;
}

std::unique_ptr<Agent> makeLargeHerbivore(AgentId id, Vec2 pos) {
    auto a = makeTickableAgent(id, pos, /*energy=*/110.0f);
    a->setDamageThreshold(12.0f);
    return a;
}
}  // namespace

TEST(SpeciesDiffL21_Threshold, SingleWolfCannotBreakHerbivoreThreshold) {
    AgentLayer* agents = nullptr;
    auto world = makeWorld(agents);

    auto wolf = makeWolf(AgentId{1}, Vec2{30, 30});
    auto* wolf_raw = wolf.get();
    auto herb = makeLargeHerbivore(AgentId{2}, Vec2{31, 30});  // 1m away
    auto* herb_raw = herb.get();
    agents->addAgent(std::move(wolf), *world);
    agents->addAgent(std::move(herb), *world);

    // Wolf hunts: damage=4 per sec, hunt_radius=5m.
    PredatorHuntCapability hunt(/*hunt_radius=*/5.0f,
                                /*damage=*/4.0f,
                                /*energy_gain=*/5.0f,
                                /*cost=*/0.02f);
    const float dt = 0.1f;
    const std::array<float, 1> outputs{1.0f};
    const float herb_initial_energy = herb_raw->energy();

    // Run 30 ticks (3s of attack). Single wolf accumulates only ~4
    // dmg in the rolling 1s window -> never breaks threshold 12.
    for (int i = 0; i < 30; ++i) {
        hunt.apply(*wolf_raw, *world, std::span<const float>(outputs), dt);
        world->tick(dt);
    }
    // Herbivore must not have lost any energy via the threshold path.
    // (We allow a tiny wiggle for rounding but the legacy direct-damage
    // path is gated off when threshold > 0.)
    EXPECT_FLOAT_EQ(herb_raw->energy(), herb_initial_energy)
        << "Single wolf should not be able to break LargeHerbivore "
           "threshold (12). Wolf damage 4/sec * 1s window = 4 < 12. "
           "If energy dropped, threshold gate is leaking.";
}

TEST(SpeciesDiffL21_Threshold, FourWolvesBreakHerbivoreThreshold) {
    // Spec § 1.2 sample: "4 wolves attacking simultaneously, accumulating
    // 16 damage" overcomes herbivore threshold 12. Test uses 4 wolves
    // to stay clear of the float-rounding boundary that 3 wolves *
    // 0.4 dmg/tick = 12.0 (exact) would hit.
    AgentLayer* agents = nullptr;
    auto world = makeWorld(agents);

    auto wolf1 = makeWolf(AgentId{1}, Vec2{30, 30});
    auto wolf2 = makeWolf(AgentId{2}, Vec2{30, 31});
    auto wolf3 = makeWolf(AgentId{3}, Vec2{31, 30});
    auto wolf4 = makeWolf(AgentId{5}, Vec2{31, 31});
    auto* wolf1_raw = wolf1.get();
    auto* wolf2_raw = wolf2.get();
    auto* wolf3_raw = wolf3.get();
    auto* wolf4_raw = wolf4.get();
    auto herb = makeLargeHerbivore(AgentId{4}, Vec2{30.5f, 30.5f});  // < 5m from each
    auto* herb_raw = herb.get();
    const float herb_initial_energy = herb_raw->energy();
    agents->addAgent(std::move(wolf1), *world);
    agents->addAgent(std::move(wolf2), *world);
    agents->addAgent(std::move(wolf3), *world);
    agents->addAgent(std::move(wolf4), *world);
    agents->addAgent(std::move(herb), *world);

    PredatorHuntCapability hunt(5.0f, 4.0f, 5.0f, 0.02f);
    const float dt = 0.1f;
    const std::array<float, 1> outputs{1.0f};

    // 4 wolves * 0.4 dmg/tick = 1.6/tick. After 8 ticks = 12.8 > 12.
    // First trigger fires; prey energy drops by ~12.8.
    for (int i = 0; i < 10; ++i) {
        hunt.apply(*wolf1_raw, *world, std::span<const float>(outputs), dt);
        hunt.apply(*wolf2_raw, *world, std::span<const float>(outputs), dt);
        hunt.apply(*wolf3_raw, *world, std::span<const float>(outputs), dt);
        hunt.apply(*wolf4_raw, *world, std::span<const float>(outputs), dt);
        world->tick(dt);
    }
    EXPECT_LT(herb_raw->energy(), herb_initial_energy)
        << "Four wolves attacking simultaneously should accumulate "
           ">= 12 in the 1s window and break the threshold.";
}

// ---- Stealth state + perception filter ----

TEST(SpeciesDiffL21_Stealth, FiltersDistantObserver) {
    AgentLayer* agents = nullptr;
    auto world = makeWorld(agents);

    auto leopard = makeAgent(AgentId{1}, Vec2{30, 30});
    leopard->setPredator(true);
    leopard->setStealth(true);
    agents->addAgent(std::move(leopard), *world);

    // Observer 5m away (outside stealth_adjacent 2m).
    auto observer = makeAgent(AgentId{2}, Vec2{35, 30});
    auto* observer_raw = observer.get();
    agents->addAgent(std::move(observer), *world);

    AgentSocialPerception perception(/*sense_radius=*/10.0f);
    std::array<float, AgentSocialPerception::kDim> out{};
    perception.sense(*observer_raw, *world, std::span<float>(out));

    // Predator filtered out: presence flag at out[7] should stay 0,
    // count at out[9] should be 0.
    EXPECT_FLOAT_EQ(out[7], 0.0f) << "Stealth predator must not appear in observer perception";
    EXPECT_FLOAT_EQ(out[9], 0.0f) << "predator_count must be 0 when only target is stealth-distant";
}

TEST(SpeciesDiffL21_Stealth, AdjacentObserverStillSeesStealth) {
    AgentLayer* agents = nullptr;
    auto world = makeWorld(agents);

    auto leopard = makeAgent(AgentId{1}, Vec2{30, 30});
    leopard->setPredator(true);
    leopard->setStealth(true);
    agents->addAgent(std::move(leopard), *world);

    // Observer 1m away, INSIDE 2m stealth_adjacent radius.
    auto observer = makeAgent(AgentId{2}, Vec2{31, 30});
    auto* observer_raw = observer.get();
    agents->addAgent(std::move(observer), *world);

    AgentSocialPerception perception(/*sense_radius=*/10.0f);
    std::array<float, AgentSocialPerception::kDim> out{};
    perception.sense(*observer_raw, *world, std::span<float>(out));

    // Adjacent observer must see the stealth predator.
    EXPECT_FLOAT_EQ(out[7], 1.0f);
    EXPECT_GT(out[9], 0.0f);
}

TEST(SpeciesDiffL21_StealthCap, EntersStealthAfterStaticDuration) {
    auto a = makeAgent(AgentId{1}, Vec2{0, 0});
    a->setVelocity(Vec2{0, 0});
    StealthCapability cap(/*stealth_threshold_seconds=*/3.0f, 0.05f);
    World2D world(WorldId{1}, Vec2{10, 10});

    // 30 ticks at dt=0.1 = 3.0s static -> stealth on.
    const float dt = 0.1f;
    for (int i = 0; i < 30; ++i) {
        cap.apply(*a, world, {}, dt);
    }
    EXPECT_TRUE(a->isStealth());

    // Move -> stealth off.
    a->setVelocity(Vec2{1.0f, 0.0f});
    cap.apply(*a, world, {}, dt);
    EXPECT_FALSE(a->isStealth());
    EXPECT_DOUBLE_EQ(a->stationarySeconds(), 0.0);
}

TEST(SpeciesDiffL21_StealthCap, BelowThresholdStaysVisible) {
    auto a = makeAgent(AgentId{1}, Vec2{0, 0});
    a->setVelocity(Vec2{0, 0});
    StealthCapability cap(3.0f, 0.05f);
    World2D world(WorldId{1}, Vec2{10, 10});
    // 20 ticks * 0.1 = 2.0s -- below 3.0s threshold.
    for (int i = 0; i < 20; ++i) {
        cap.apply(*a, world, {}, 0.1f);
    }
    EXPECT_FALSE(a->isStealth());
}

// ---- Cave hide perception filter ----

TEST(SpeciesDiffL21_CaveHide, OutsideObserverCannotSeePreyInCave) {
    AgentLayer* agents = nullptr;
    auto world = makeWorld(agents);

    // Use a non-ProtagonistBrain agent for the prey - otherwise
    // AgentSocialPerception classifies it as a protagonist peer
    // (counted at out[8]) instead of as nearest_prey (presence flag
    // out[3]).
    auto prey = makeTickableAgent(AgentId{1}, Vec2{30, 30});
    prey->setInCave(true);
    agents->addAgent(std::move(prey), *world);

    // Observer (protagonist) outside the cave. Predator-flag false,
    // brain is ProtagonistBrain so makeAgent path is fine here.
    auto observer = makeAgent(AgentId{2}, Vec2{32, 30});
    observer->setInCave(false);
    auto* observer_raw = observer.get();
    agents->addAgent(std::move(observer), *world);

    AgentSocialPerception perception(/*sense_radius=*/10.0f);
    std::array<float, AgentSocialPerception::kDim> out{};
    perception.sense(*observer_raw, *world, std::span<float>(out));
    // Prey filtered: presence flag at out[3] should stay 0.
    EXPECT_FLOAT_EQ(out[3], 0.0f);
}

TEST(SpeciesDiffL21_CaveHide, CavemateObserverCanSeePreyInCave) {
    AgentLayer* agents = nullptr;
    auto world = makeWorld(agents);

    auto prey = makeTickableAgent(AgentId{1}, Vec2{30, 30});
    prey->setInCave(true);
    agents->addAgent(std::move(prey), *world);

    auto observer = makeAgent(AgentId{2}, Vec2{31, 30});
    observer->setInCave(true);  // both in cave
    auto* observer_raw = observer.get();
    agents->addAgent(std::move(observer), *world);

    AgentSocialPerception perception(10.0f);
    std::array<float, AgentSocialPerception::kDim> out{};
    perception.sense(*observer_raw, *world, std::span<float>(out));
    EXPECT_FLOAT_EQ(out[3], 1.0f);
}

// ---- Digest satiety ----

TEST(SpeciesDiffL21_DigestSatiety, ApexEntersSatietyAfterThresholdKill) {
    AgentLayer* agents = nullptr;
    auto world = makeWorld(agents);

    auto apex = makeTickableAgent(AgentId{1}, Vec2{30, 30}, /*energy=*/200.0f);
    apex->setPredator(true);
    apex->setDietPreference(0.95f);  // >0.85 marks as apex for satiety arming
    auto* apex_raw = apex.get();

    // Weak prey with high threshold -- apex one-shots threshold-class
    // prey when its damage * dt accumulates above threshold within
    // the 1s window.
    auto prey = makeTickableAgent(AgentId{2}, Vec2{31, 30}, /*energy=*/13.0f);
    prey->setDamageThreshold(12.0f);
    auto* prey_raw = prey.get();
    agents->addAgent(std::move(apex), *world);
    agents->addAgent(std::move(prey), *world);

    PredatorHuntCapability hunt(/*hunt_radius=*/5.0f,
                                /*damage=*/18.0f,  // apex stat
                                /*energy_gain=*/14.0f,
                                /*cost=*/0.025f);
    const float dt = 0.1f;
    const std::array<float, 1> outputs{1.0f};

    // After ~1s: window total ~= 18 dmg/sec * (1s) ~= 18 > 12 ->
    // threshold breaks at some tick. Prey energy 13 - 18 = -5 -> kill.
    for (int i = 0; i < 20 && prey_raw->alive(); ++i) {
        hunt.apply(*apex_raw, *world, std::span<const float>(outputs), dt);
        world->tick(dt);
    }
    EXPECT_FALSE(prey_raw->alive());
    // After kill, apex must be in digest satiety for ~60s.
    EXPECT_TRUE(apex_raw->inDigestSatiety(world->currentTimeSeconds()));
    EXPECT_GT(apex_raw->digestSatietyUntilSeconds(),
              world->currentTimeSeconds() + 50.0);
}

// ---- Scavenge efficiency ----

TEST(SpeciesDiffL21_Scavenge, BonusGainOnLowVitalityPrey) {
    AgentLayer* agents = nullptr;
    auto world = makeWorld(agents);

    auto scav = makeAgent(AgentId{1}, Vec2{30, 30}, /*energy=*/100.0f);
    scav->setPredator(true);
    scav->setScavengeEfficiency(3.0f);
    scav->setDietPreference(0.85f);
    auto* scav_raw = scav.get();

    // Low-vitality prey: energy = 1 (well below threshold * 0.25 = 3.0).
    auto prey = makeAgent(AgentId{2}, Vec2{31, 30}, /*energy=*/1.0f);
    prey->setDamageThreshold(12.0f);
    auto* prey_raw = prey.get();
    agents->addAgent(std::move(scav), *world);
    agents->addAgent(std::move(prey), *world);

    const float scav_initial = scav_raw->energy();
    PredatorHuntCapability hunt(4.0f, 7.0f, 5.0f, 0.018f);
    const float dt = 0.1f;
    const std::array<float, 1> outputs{1.0f};
    hunt.apply(*scav_raw, *world, std::span<const float>(outputs), dt);

    // Scavenger should have gained more than the base per-tick gain
    // (which would be ~5 * 0.1 * dietBoost = ~0.55). The 3x scav
    // multiplier on low-vit prey should push it well above that.
    const float gained = scav_raw->energy() - scav_initial + 0.018f * dt;
    EXPECT_GT(gained, 1.0f)
        << "Scavenger should benefit from scavenge_efficiency bonus on low-vitality prey";
}

TEST(SpeciesDiffL21_Scavenge, PenaltyOnHealthyPrey) {
    AgentLayer* agents = nullptr;
    auto world = makeWorld(agents);

    auto scav = makeAgent(AgentId{1}, Vec2{30, 30}, /*energy=*/100.0f);
    scav->setPredator(true);
    scav->setScavengeEfficiency(3.0f);
    scav->setDietPreference(0.85f);
    auto* scav_raw = scav.get();
    auto prey = makeAgent(AgentId{2}, Vec2{31, 30}, /*energy=*/95.0f);
    prey->setDamageThreshold(12.0f);  // 25% floor = 3.0
    agents->addAgent(std::move(scav), *world);
    agents->addAgent(std::move(prey), *world);

    const float scav_initial = scav_raw->energy();
    PredatorHuntCapability hunt(4.0f, 7.0f, 5.0f, 0.018f);
    const float dt = 0.1f;
    const std::array<float, 1> outputs{1.0f};
    hunt.apply(*scav_raw, *world, std::span<const float>(outputs), dt);

    // Healthy prey: scavenger pays a 1/3 tax. Per-tick gain should be
    // SMALL (~0.18), much less than the bonus path above.
    const float gained = scav_raw->energy() - scav_initial + 0.018f * dt;
    EXPECT_LT(gained, 0.5f)
        << "Scavenger should pay tax (1/scav_eff) on healthy prey";
}
