// Layer 6 R3 stage 4 (LAYER6_THREE_WORLDS_SPEC § 1.3 + § 2.5,
// PRODUCTION_WORLD_LIFECYCLE_SPEC invariant #1) tests for the
// persistent MainWorldHost:
//   - host owns a non-null World2D after construction
//   - tick() advances sim time monotonically
//   - tick() with non-positive dt is a no-op and the world's clock
//     does not regress (i.e. "永不 reset")
//   - host can be ticked many times without recreating the world
//     (the underlying World2D pointer stays the same)
//   - tickCallCount() counts only successful ticks

#include "runtime/MainWorldHost.h"
#include "runtime/ProtagonistEcologyConfig.h"
#include "core/world/World2D.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

using neuro::routes::protagonist::MainWorldHost;
using neuro::routes::protagonist::ProtagonistEcologyConfig;

ProtagonistEcologyConfig makeMinimalConfig() {
    // The defaults from ProtagonistEcologyConfig are deliberately
    // conservative (small world, all optional layers off unless
    // explicitly enabled in toml). createBootstrapWorld respects
    // those defaults so we get a tiny World2D suitable for a unit
    // test (no GPU, no NEAT, no agents seeded).
    ProtagonistEcologyConfig route;
    route.world_size = neuro::Vec2{100.0f, 100.0f};
    route.random_seed = 42;
    return route;
}

}  // namespace

TEST(MainWorldHostContinuousTick, ConstructionYieldsWorld) {
    auto cfg = makeMinimalConfig();
    MainWorldHost host(cfg);
    EXPECT_NE(host.world(), nullptr)
        << "MainWorldHost should own a World2D after construction";
    EXPECT_DOUBLE_EQ(host.simTimeSeconds(), 0.0);
    EXPECT_EQ(host.tickCallCount(), 0u);
    EXPECT_EQ(host.seedCount(), 0u)
        << "MainWorldHost should not seed any agents until seedAgent() is "
        << "called (the persistent World2D starts empty — only borderland→"
        << "main_world promotions populate it, per L6 R3 stage 4b)";
}

TEST(MainWorldHostContinuousTick, TickAdvancesSimTime) {
    auto cfg = makeMinimalConfig();
    MainWorldHost host(cfg);
    const auto t0 = host.simTimeSeconds();

    ASSERT_TRUE(host.tick(0.1));
    const auto t1 = host.simTimeSeconds();
    EXPECT_GT(t1, t0);

    ASSERT_TRUE(host.tick(0.25));
    const auto t2 = host.simTimeSeconds();
    EXPECT_GT(t2, t1);

    EXPECT_EQ(host.tickCallCount(), 2u);
}

TEST(MainWorldHostContinuousTick, NonPositiveDtIsNoop) {
    auto cfg = makeMinimalConfig();
    MainWorldHost host(cfg);
    ASSERT_TRUE(host.tick(0.5));
    const auto t_after_real_tick = host.simTimeSeconds();
    const auto count_after_real_tick = host.tickCallCount();

    EXPECT_FALSE(host.tick(0.0));
    EXPECT_FALSE(host.tick(-1.0));
    EXPECT_DOUBLE_EQ(host.simTimeSeconds(), t_after_real_tick)
        << "Non-positive dt must not advance sim time (永不 reset && "
        << "no negative drift)";
    EXPECT_EQ(host.tickCallCount(), count_after_real_tick);
}

TEST(MainWorldHostContinuousTick, NeverRecreatesWorldAcrossManyTicks) {
    auto cfg = makeMinimalConfig();
    MainWorldHost host(cfg);
    const auto* world_ptr_before = host.world();
    ASSERT_NE(world_ptr_before, nullptr);

    // Spec § 1.3 invariant #1: the main world is "永不 reset". This
    // is the structural test for it — after N ticks, the underlying
    // World2D pointer must be identical to the original.
    for (int i = 0; i < 50; ++i) {
        ASSERT_TRUE(host.tick(0.05));
    }
    const auto* world_ptr_after = host.world();
    EXPECT_EQ(world_ptr_before, world_ptr_after)
        << "World2D instance must be reused across all tick() calls";
    EXPECT_EQ(host.tickCallCount(), 50u);
    EXPECT_GT(host.simTimeSeconds(), 0.0);
}

TEST(MainWorldHostContinuousTick, SimTimeMatchesWorldClock) {
    auto cfg = makeMinimalConfig();
    MainWorldHost host(cfg);
    ASSERT_TRUE(host.tick(0.3));
    ASSERT_TRUE(host.tick(0.7));
    // The host delegates to the World2D's own monotonic clock to
    // avoid double-bookkeeping; verify the values are identical.
    EXPECT_DOUBLE_EQ(host.simTimeSeconds(),
                     host.world()->currentTimeSeconds());
}

// L6 R3 stage 4c: minimum invariants of the agent snapshot writer.
// We don't seed any agents here (the snapshot path is reachable even
// when the main world is empty — and an empty manifest is itself a
// useful baseline for the future Layer 7 viewer to recognise "no
// migration occurred"). The richer end-to-end test (after a real
// 80-gen run seeds ~300 agents) lives in the smoke/long-run scripts.
TEST(MainWorldHostStage4c, EmptyHostSnapshotProducesValidZeroAgentManifest) {
    auto cfg = makeMinimalConfig();
    MainWorldHost host(cfg);
    ASSERT_TRUE(host.tick(0.5));
    ASSERT_TRUE(host.tick(0.5));

    const auto tmp_dir = std::filesystem::temp_directory_path()
                       / "main_world_host_test_snapshot_empty";
    std::error_code ec;
    std::filesystem::remove_all(tmp_dir, ec);

    const std::size_t saved = host.saveAgentsSnapshot(tmp_dir);
    EXPECT_EQ(saved, 0u)
        << "An empty main world should write a manifest containing zero agents";

    const auto manifest = tmp_dir / "main_world_agents.json";
    EXPECT_TRUE(std::filesystem::exists(manifest))
        << "saveAgentsSnapshot must create the manifest file";

    std::ifstream in(manifest);
    std::ostringstream body;
    body << in.rdbuf();
    const std::string contents = body.str();
    // Spot-check that all the documented top-level fields are present
    // and that the agents array is empty. We deliberately avoid pulling
    // in a JSON dependency for unit testing — the runtime hand-writes
    // the manifest so substring checks are sufficient for shape.
    EXPECT_NE(contents.find("\"version\""), std::string::npos);
    EXPECT_NE(contents.find("\"snapshot_sim_time\""), std::string::npos);
    EXPECT_NE(contents.find("\"tick_call_count\""), std::string::npos);
    EXPECT_NE(contents.find("\"cumulative_seed_count\""), std::string::npos);
    EXPECT_NE(contents.find("\"agent_count\": 0"), std::string::npos);
    EXPECT_NE(contents.find("\"agents\": []"), std::string::npos);

    std::filesystem::remove_all(tmp_dir, ec);
}

TEST(MainWorldHostStage4c, SnapshotIntoNonExistentDirCreatesIt) {
    auto cfg = makeMinimalConfig();
    MainWorldHost host(cfg);

    const auto tmp_dir = std::filesystem::temp_directory_path()
                       / "main_world_host_test_snapshot_mkdir"
                       / "nested" / "subdir";
    std::error_code ec;
    std::filesystem::remove_all(
        std::filesystem::temp_directory_path()
            / "main_world_host_test_snapshot_mkdir",
        ec);

    EXPECT_FALSE(std::filesystem::exists(tmp_dir));
    const std::size_t saved = host.saveAgentsSnapshot(tmp_dir);
    EXPECT_EQ(saved, 0u);
    EXPECT_TRUE(std::filesystem::exists(tmp_dir))
        << "saveAgentsSnapshot must recursively create the snapshot directory";
    EXPECT_TRUE(std::filesystem::exists(tmp_dir / "main_world_agents.json"));

    std::filesystem::remove_all(
        std::filesystem::temp_directory_path()
            / "main_world_host_test_snapshot_mkdir",
        ec);
}
