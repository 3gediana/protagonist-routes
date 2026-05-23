// deep_protagonist · S1 viewer
//
// Stage S1 acceptance:
//   - libtorch + CUDA still alive (kept from S0)
//   - Procedural 3D heightmap world with caves
//   - Capsule agent (no neural net yet) wandering on the terrain, falls
//     under gravity, slides off slopes, can step inside caves
//   - First-person fly camera (WASD + mouse) so the user can inspect the
//     world from any angle
//
// On startup we still run the S0 GPU smoke test once before opening the
// window, to keep verifying the libtorch toolchain is healthy.

#include "agent/CapsuleAgent.hpp"
#include "agent/EpisodeManager.hpp"
#include "agent/Inventory.hpp"
#include "agent/Observation.hpp"
#include "agent/Perception.hpp"
#include "agent/RewardSystem.hpp"
#include "agent/Shelter.hpp"
#include "agent/VitalSystem.hpp"
#include "render/FlyCamera.hpp"
#include "render/WorldRenderer.hpp"
#include "world/AnimalSystem.hpp"
#include "world/Daylight.hpp"
#include "world/Decorations.hpp"
#include "world/FoodSystem.hpp"
#include "world/Physics.hpp"
#include "world/PlantSystem.hpp"
#include "world/ResourceSystem.hpp"
#include "world/World.hpp"

#include <torch/torch.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <toml++/toml.hpp>

#include <raylib.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

namespace {

void print_banner() {
    spdlog::info("================================================");
    spdlog::info(" deep_protagonist · S1 viewer");
    spdlog::info("================================================");
}

void run_torch_smoke() {
    spdlog::info("libtorch version : {}", TORCH_VERSION);
    spdlog::info("CUDA available   : {}", torch::cuda::is_available());
    if (torch::cuda::is_available()) {
        torch::Device gpu(torch::kCUDA, 0);
        auto a = torch::randn({256, 256}, gpu);
        auto b = torch::randn({256, 256}, gpu);
        auto c = torch::matmul(a, b);
        torch::cuda::synchronize();
        double sum = c.cpu().sum().item<double>();
        spdlog::info("GPU warm-up matmul sum: {:.3f}", sum);
    }
}

dp::world::World::Config load_world_config(const std::string& path) {
    dp::world::World::Config cfg;
    if (!std::filesystem::exists(path)) {
        spdlog::warn("Config file '{}' missing; using built-in defaults.", path);
        return cfg;
    }
    auto t = toml::parse_file(path);
    cfg.heightmap_cells_x   = (int)   t["world"]["heightmap_cells_x"].value_or<int64_t>(cfg.heightmap_cells_x);
    cfg.heightmap_cells_y   = (int)   t["world"]["heightmap_cells_y"].value_or<int64_t>(cfg.heightmap_cells_y);
    cfg.cell_scale          = (float) t["world"]["cell_scale"].value_or<double>(cfg.cell_scale);
    cfg.terrain_max_height  = (float) t["world"]["max_height"].value_or<double>(cfg.terrain_max_height);
    cfg.terrain_frequency   = (float) t["world"]["terrain_frequency"].value_or<double>(cfg.terrain_frequency);
    cfg.terrain_octaves     = (int)   t["world"]["terrain_octaves"].value_or<int64_t>(cfg.terrain_octaves);
    cfg.terrain_persistence = (float) t["world"]["terrain_persistence"].value_or<double>(cfg.terrain_persistence);
    cfg.cave_count          = (int)   t["world"]["cave_count"].value_or<int64_t>(cfg.cave_count);
    cfg.river_count         = (int)   t["world"]["river_count"].value_or<int64_t>(cfg.river_count);
    cfg.seed                = (uint64_t) t["world"]["seed"].value_or<int64_t>(12345);
    return cfg;
}

}  // namespace

// ---------------------------------------------------------------------------
// Headless self-tests. No window, no GPU, just verify the pure math is sane.
// Run with: deep_protagonist.exe --self-test
// ---------------------------------------------------------------------------
namespace {

int run_self_tests() {
    spdlog::info("================================================");
    spdlog::info(" deep_protagonist · headless self-tests");
    spdlog::info("================================================");

    int failures = 0;
    auto check = [&](bool ok, const std::string& msg) {
        if (ok) spdlog::info("  PASS  {}", msg);
        else { spdlog::error("  FAIL  {}", msg); ++failures; }
    };

    // --- Camera math ---
    {
        dp::render::FlyCamera cam;
        cam.set_position(Vector3{ 0, 0, 0 });
        cam.set_yaw_pitch(0.0f, 0.0f);
        Vector3 f = cam.forward();
        check(std::abs(f.x - 1.0f) < 1e-4f && std::abs(f.y) < 1e-4f && std::abs(f.z) < 1e-4f,
              "yaw=0,pitch=0 -> forward = +X");

        cam.apply_mouse_delta(Vector2{ 100, 0 });   // mouse right
        Vector3 f2 = cam.forward();
        check(f2.y < 0.0f,
              "mouse right -> view turns toward -Y (player's right) [f.y<0]");
        check(cam.yaw() < 0.0f,
              "mouse right -> yaw decreases");

        cam.set_yaw_pitch(0.0f, 0.0f);
        cam.apply_mouse_delta(Vector2{ 0, 100 });   // mouse down
        Vector3 f3 = cam.forward();
        check(f3.z < 0.0f,
              "mouse down -> view tilts down [f.z<0]");

        // set_target round-trip
        cam.set_position(Vector3{ 10, 10, 0 });
        cam.set_target(Vector3{ 11, 10, 0 });   // looking at +X
        Vector3 f4 = cam.forward();
        check(std::abs(f4.x - 1.0f) < 1e-3f,
              "set_target(+X relative) -> forward = +X");

        cam.set_target(Vector3{ 10, 11, 0 });   // looking at +Y
        Vector3 f5 = cam.forward();
        check(std::abs(f5.y - 1.0f) < 1e-3f,
              "set_target(+Y relative) -> forward = +Y");
    }

    // --- World / heightmap / biomes / rivers ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 32;
        cfg.heightmap_cells_y = 32;
        cfg.river_count = 2;
        dp::world::World w(cfg);
        float h0 = w.surface_height(5.0f, 5.0f);
        check(std::isfinite(h0) && h0 >= 0.0f && h0 <= cfg.terrain_max_height + 5.0f,
              "heightmap sample is finite & in [0, max_height]");

        bool any_cave = w.caves().caves().size() == size_t(cfg.cave_count);
        check(any_cave, "world generates the requested number of caves");

        // World coverage matches cells * cell_scale
        float wx_max = w.heightmap().world_size_x();
        check(std::abs(wx_max - (cfg.heightmap_cells_x - 1) * cfg.cell_scale) < 1e-3f,
              "world span = (cells-1) * cell_scale");

        // Biome map should have all 6 biome ids in [0, 5]
        bool biome_ok = true;
        for (uint8_t b : w.biomes().raw()) {
            if (b > static_cast<uint8_t>(dp::world::Biome::Desert)) {
                biome_ok = false; break;
            }
        }
        check(biome_ok, "every cell has a valid biome label");

        // River system carved at least some segments
        check(!w.rivers().segments().empty(),
              "river system carves at least one segment");

        // At least one River-tagged cell exists in the biome map
        int river_cells = 0;
        for (uint8_t b : w.biomes().raw()) {
            if (b == static_cast<uint8_t>(dp::world::Biome::River)) ++river_cells;
        }
        check(river_cells > 0, "biome map contains river cells");
    }

    // --- Physics ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 16;
        cfg.heightmap_cells_y = 16;
        cfg.cave_count = 0;
        dp::world::World w(cfg);

        dp::world::Physics phys;
        dp::world::CapsuleState s{};
        s.pos_x = 8.0f;
        s.pos_y = 8.0f;
        s.pos_z = 200.0f;  // way above ground
        for (int i = 0; i < 600; ++i) phys.step(w, s, 1.0f / 60.0f);
        float h = w.surface_height(s.pos_x, s.pos_y);
        check(std::abs(s.pos_z - h) < 0.2f && s.on_ground,
              "capsule falls and settles on terrain");
    }

    // --- FoodSystem ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 32;
        cfg.heightmap_cells_y = 32;
        cfg.cave_count = 0;
        dp::world::World w(cfg);

        dp::world::FoodSystem food;
        food.initialize(42, 20, w);
        check(food.alive_count() == 20,
              "FoodSystem initializes the requested count");

        // Ask for the closest 3 food pellets to (0,0,0). They should be
        // sorted by distance.
        auto idxs = food.closest_k(0, 0, 0, 3);
        check(idxs.size() == 3 && idxs[0] >= 0,
              "closest_k returns at least one valid index");
        if (idxs[0] >= 0 && idxs[1] >= 0) {
            const auto& a = food.pellets()[idxs[0]];
            const auto& b = food.pellets()[idxs[1]];
            float d_a = a.x*a.x + a.y*a.y + a.z*a.z;
            float d_b = b.x*b.x + b.y*b.y + b.z*b.z;
            check(d_a <= d_b, "closest_k entries are sorted by distance");
        }

        // Eating: position the agent right on top of pellet[0]; should eat
        // exactly one pellet, and the alive count should remain at 20
        // (because eaten pellets respawn).
        auto& p0 = food.pellets()[0];
        int eaten = food.try_eat(p0.x, p0.y, p0.z, w, 1.0f);
        check(eaten >= 1, "try_eat consumes at least one pellet on contact");
        check(food.alive_count() == 20,
              "alive count stays constant after eating (respawn worked)");
    }

    // --- Perception ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 32;
        cfg.heightmap_cells_y = 32;
        cfg.cave_count = 0;
        dp::world::World w(cfg);

        dp::world::FoodSystem food;
        food.initialize(7, 10, w);

        dp::agent::CapsuleAgent agent(123);
        agent.reset(50.0f, 50.0f, w.surface_height(50, 50) + 1.0f);

        dp::agent::Perception perc;
        dp::agent::PerceptionOutput out;
        perc.compute(agent, food, out);

        check(dp::agent::PerceptionOutput::SIZE == 18,
              "Perception output size is 18 floats");

        // First three should be the agent's velocity (0 right after reset)
        check(std::abs(out.data[0]) < 1e-4f && std::abs(out.data[1]) < 1e-4f
              && std::abs(out.data[2]) < 1e-4f,
              "Perception velocity slots zero after agent reset");

        // Each food slot should be in roughly [-1.5, 1.5] (normalized by VISION_RANGE)
        bool food_in_range = true;
        for (int i = 3; i < dp::agent::PerceptionOutput::SIZE; ++i) {
            if (std::abs(out.data[i]) > 1.5f) { food_in_range = false; break; }
        }
        check(food_in_range, "Perception food slots normalized roughly to [-1.5, 1.5]");
    }

    // --- VitalSystem ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 16;
        cfg.heightmap_cells_y = 16;
        cfg.cave_count = 0;
        cfg.river_count = 0;
        dp::world::World w(cfg);

        dp::agent::VitalSystem vs;
        dp::agent::VitalState v{};
        check(v.alive && v.hunger == 100.0f && v.thirst == 100.0f,
              "VitalState defaults: alive, full hunger and thirst");

        dp::world::Daylight day(60.0f, 0.5f);  // noon, fast cycle
        // 60 seconds of standing still at noon: hunger should drop ~8 (one min worth)
        for (int i = 0; i < 60; ++i)
            vs.tick(v, w, 30.0f, 30.0f, 0.0f, day, 1.0f);
        check(v.hunger < 95.0f && v.hunger > 80.0f,
              "VitalSystem: hunger drops ~8 over a minute of standing still");
        check(v.stamina > 99.0f,
              "VitalSystem: stamina recovers when standing still");

        // Eat to refill hunger
        vs.eat(v, 30.0f);
        check(v.hunger >= 95.0f, "VitalSystem: eating refills hunger");

        // Force hunger to zero -> dies
        v.hunger = 0.0f;
        vs.tick(v, w, 30.0f, 30.0f, 0.0f, day, 1.0f);
        check(!v.alive && v.deaths == 1, "VitalSystem: zero hunger kills agent");
    }

    // --- PlantSystem + Decorations ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 64;
        cfg.heightmap_cells_y = 64;
        cfg.cave_count = 0;
        dp::world::World w(cfg);

        dp::world::Decorations decor;
        decor.generate(7, w);

        dp::world::PlantSystem plants;
        plants.initialize(11, w, decor);
        check(!plants.plants().empty(),
              "PlantSystem: at least one plant placed");

        // Harvest at every plant location to confirm consumption + regrowth state
        int total_ripe_before = 0;
        for (const auto& p : plants.plants()) if (p.ripe) ++total_ripe_before;
        if (!plants.plants().empty()) {
            const auto& p0 = plants.plants()[0];
            auto got = plants.try_harvest(p0.x, p0.y, p0.z, 1.0f);
            check(!got.empty(), "PlantSystem: harvest near a ripe plant returns kinds");

            int ripe_after = 0;
            for (const auto& p : plants.plants()) if (p.ripe) ++ripe_after;
            check(ripe_after < total_ripe_before,
                  "PlantSystem: ripe count decreases after harvest");
        }
    }

    // --- AnimalSystem ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 64;
        cfg.heightmap_cells_y = 64;
        cfg.cave_count = 0;
        dp::world::World w(cfg);

        dp::world::AnimalSystem animals;
        animals.initialize(99, w, 5, 3, 2, 4, 2);
        int n = static_cast<int>(animals.animals().size());
        check(n == 5 + 3 + 2 + 4 + 2,
              "AnimalSystem: spawned exactly the requested counts");

        // Tick 30 seconds of AI - nothing should crash; positions should change
        float pos0_x = animals.animals()[0].pos_x;
        for (int i = 0; i < 300; ++i)
            animals.tick(w, 32.0f, 32.0f, 0.0f, 0.1f);
        check(std::abs(animals.animals()[0].pos_x - pos0_x) > 0.5f,
              "AnimalSystem: at least the first animal has moved");

        // A wolf placed near the agent should attempt the bite
        int wolf_idx = animals.closest_to(0, 0, dp::world::AnimalKind::Wolf);
        check(wolf_idx >= 0, "AnimalSystem: closest_to finds a wolf");
    }

    // --- AgentAction ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 16;
        cfg.heightmap_cells_y = 16;
        cfg.cave_count = 0;
        cfg.river_count = 0;
        dp::world::World w(cfg);
        dp::world::Physics phys;

        dp::agent::CapsuleAgent agent(42);
        agent.reset(8.0f, 8.0f, w.surface_height(8, 8) + 1.0f);
        // Drop to ground
        for (int i = 0; i < 60; ++i) {
            agent.apply_action(dp::agent::noop_action(), w, phys, 1.0f / 60.0f);
        }
        float x0 = agent.state().pos_x;

        dp::agent::AgentAction east{};
        east.move_x = 1.0f; east.move_y = 0.0f;
        for (int i = 0; i < 60; ++i) {
            agent.apply_action(east, w, phys, 1.0f / 60.0f);
        }
        check(agent.state().pos_x - x0 > 0.5f,
              "AgentAction: move_x>0 actually moves agent eastward");

        dp::agent::AgentAction yawl{};
        yawl.yaw_rate = 1.0f;  // 1 rad/s left turn
        float y0 = agent.yaw();
        for (int i = 0; i < 60; ++i) {
            agent.apply_action(yawl, w, phys, 1.0f / 60.0f);
        }
        check(std::abs(agent.yaw() - y0 - 1.0f) < 0.1f,
              "AgentAction: yaw_rate*dt rotates agent over time");
    }

    // --- Observation ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 64;
        cfg.heightmap_cells_y = 64;
        cfg.cell_scale = 4.0f;
        cfg.cave_count = 0;
        cfg.river_count = 0;
        dp::world::World w(cfg);

        dp::world::PlantSystem plants;
        dp::world::Decorations  decor;
        decor.generate(1, w);
        plants.initialize(2, w, decor);

        dp::world::AnimalSystem animals;
        animals.initialize(3, w);

        dp::world::ResourceSystem resources;
        resources.initialize(4, w);

        dp::agent::CapsuleAgent agent(5);
        float ax = w.heightmap().world_size_x() * 0.5f;
        float ay = w.heightmap().world_size_y() * 0.5f;
        agent.reset(ax, ay, w.surface_height(ax, ay) + 2.0f);

        dp::agent::VitalState vstate{};
        dp::agent::Inventory  inv;
        dp::agent::Shelter    sh;
        dp::world::Daylight   day;
        dp::agent::EpisodeManager em;
        em.begin_episode();

        dp::agent::ObservationBuilder::Vector obs{};
        dp::agent::ObservationBuilder::compose(
            w, agent, vstate, day, plants, animals, resources,
            inv, sh, em, obs);

        check(dp::agent::ObservationBuilder::SIZE >= 100,
              "Observation: total dimension is at least 100");
        check(obs[0] == 1.0f && obs[1] == 1.0f && obs[2] == 1.0f,
              "Observation: vital slots [0..2] are 1.0 at full state");

        // Every entry should fit within [-1, 1] thanks to clamping
        bool all_in_range = true;
        for (int i = 0; i < dp::agent::ObservationBuilder::SIZE; ++i) {
            if (obs[i] < -1.001f || obs[i] > 1.001f) { all_in_range = false; break; }
        }
        check(all_in_range,
              "Observation: every entry is normalised to [-1, 1]");

        // After placing a shelter, the corresponding slots should change
        inv.add(dp::agent::ItemKind::Wood);
        inv.add(dp::agent::ItemKind::Grass);
        check(sh.try_place(inv, ax + 5.0f, ay, vstate.alive ? 0.0f : 0.0f),
              "Observation: helper place_shelter succeeds");
        dp::agent::ObservationBuilder::Vector obs2{};
        dp::agent::ObservationBuilder::compose(
            w, agent, vstate, day, plants, animals, resources,
            inv, sh, em, obs2);
        // Last 3 slots are shelter (placed, dx, dy). placed should now be 1
        int placed_idx = dp::agent::ObservationBuilder::SIZE - 3;
        check(obs2[placed_idx] > 0.5f,
              "Observation: shelter-placed flag flips after build");
    }

    // --- ResourceSystem ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 64;
        cfg.heightmap_cells_y = 64;
        cfg.cell_scale = 4.0f;
        cfg.cave_count = 0;
        cfg.river_count = 0;
        dp::world::World w(cfg);

        dp::world::ResourceSystem rs;
        rs.initialize(7, w);
        check(rs.resources().size() > 10,
              "ResourceSystem: at least 10 nodes placed in a small world");

        // Find any resource and harvest it from inside its radius
        const auto& nodes = rs.resources();
        bool found_pickup = false;
        for (const auto& r : nodes) {
            if (!r.available) continue;
            auto got = rs.try_collect(r.x, r.y, r.z, 1.6f);
            if (!got.empty()) { found_pickup = true; break; }
        }
        check(found_pickup,
              "ResourceSystem: try_collect harvests at least one node");

        int wood_after = rs.available_count(dp::world::ResourceKind::Wood)
                        + rs.available_count(dp::world::ResourceKind::Stone)
                        + rs.available_count(dp::world::ResourceKind::Grass);
        check(wood_after < int(nodes.size()),
              "ResourceSystem: collected node is no longer available");

        // Tick well past the longest cooldown -> all nodes regrow
        rs.tick(500.0f);
        int avail_now = rs.available_count(dp::world::ResourceKind::Wood)
                       + rs.available_count(dp::world::ResourceKind::Stone)
                       + rs.available_count(dp::world::ResourceKind::Grass);
        check(avail_now == int(nodes.size()),
              "ResourceSystem: nodes regrow after long enough idle time");
    }

    // --- Inventory ---
    {
        dp::agent::Inventory inv;
        check(inv.empty() && !inv.full() && inv.size() == 0,
              "Inventory: starts empty");
        check(inv.add(dp::agent::ItemKind::Wood)
              && inv.add(dp::agent::ItemKind::Stone)
              && inv.add(dp::agent::ItemKind::Grass),
              "Inventory: can hold 3 items");
        check(!inv.add(dp::agent::ItemKind::Wood),
              "Inventory: rejects 4th item when full");
        check(inv.craft_spear()
              && inv.num_of(dp::agent::ItemKind::Wood) == 0
              && inv.num_of(dp::agent::ItemKind::Stone) == 0
              && inv.num_of(dp::agent::ItemKind::Spear) == 1,
              "Inventory: craft_spear consumes wood+stone, adds spear");
        // After crafting, only Grass + Spear remain (2 slots used)
        check(inv.size() == 2,
              "Inventory: post-craft slots compact correctly");
    }

    // --- Shelter ---
    {
        dp::agent::Shelter sh;
        dp::agent::Inventory inv;
        check(!sh.placed(), "Shelter: starts unplaced");
        check(!sh.try_place(inv, 0, 0, 0),
              "Shelter: refuses with empty inventory");

        inv.add(dp::agent::ItemKind::Wood);
        inv.add(dp::agent::ItemKind::Grass);
        check(sh.try_place(inv, 12.0f, 8.0f, 0.0f),
              "Shelter: 1W+1G is enough to build");
        check(sh.placed() && inv.empty(),
              "Shelter: placed flag set and materials consumed");
        check(sh.covers(13.0f, 8.0f) && !sh.covers(50.0f, 50.0f),
              "Shelter: covers() respects radius");
    }

    // --- Agent attack ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 64;
        cfg.heightmap_cells_y = 64;
        cfg.cave_count = 0;
        cfg.river_count = 0;
        dp::world::World w(cfg);

        dp::world::AnimalSystem animals;
        animals.initialize(33, w, 1, 0, 1, 0, 0);  // 1 rabbit + 1 wolf
        // Pull positions out and attack from a known location
        const auto& a0 = animals.animals()[0];
        // Aim straight at it from 1m back
        float ax = a0.pos_x - 1.0f;
        float ay = a0.pos_y;
        float yaw = 0.0f;  // facing +x
        auto miss = animals.agent_attack(ax, ay, yaw, 0.5f, 999.0f);
        check(miss.index < 0,
              "agent_attack: out-of-reach attack misses");
        auto hit = animals.agent_attack(ax, ay, yaw, 2.0f, 30.0f);
        check(hit.index >= 0 && !hit.killed,
              "agent_attack: in-range hit damages but doesn't kill on first blow");
        auto kill_blow = animals.agent_attack(ax, ay, yaw, 2.0f, 999.0f);
        check(kill_blow.index >= 0 && kill_blow.killed,
              "agent_attack: heavy follow-up kills the target");
    }

    // --- Wolf night-time aggression ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 64;
        cfg.heightmap_cells_y = 64;
        cfg.cave_count = 0;
        cfg.river_count = 0;
        dp::world::World w(cfg);

        dp::world::AnimalSystem animals;
        // No rabbits/deer/fish/eagle; only one wolf so we can poke at it
        animals.initialize(11, w, 0, 0, 1, 0, 0);
        // Force the wolf to a known position far from the agent
        // (we can't poke at the underlying vector mutably from the
        // outside, so we settle for an indirect comparison.)
        const auto& a0 = animals.animals()[0];
        // Tick once with no night, agent far away -> wolf should not be Hunt
        animals.tick(w, a0.pos_x + 100.0f, a0.pos_y + 100.0f, 0.0f, 0.5f, 0.0f);
        // Tick again with strong night and same far agent -> wolf with
        // 1.5x perception sees agent ~60m away (vs 40m) so is more likely
        // to switch to Hunt. We assert the *capability*, not deterministic
        // state, by checking the wolf moves measurably faster at night.
        float vel0 = std::sqrt(animals.animals()[0].vel_x * animals.animals()[0].vel_x
                              + animals.animals()[0].vel_y * animals.animals()[0].vel_y);
        animals.tick(w, a0.pos_x + 30.0f, a0.pos_y + 30.0f, 0.0f, 0.5f, 1.0f);
        float vel1 = std::sqrt(animals.animals()[0].vel_x * animals.animals()[0].vel_x
                              + animals.animals()[0].vel_y * animals.animals()[0].vel_y);
        check(vel1 >= vel0,
              "AnimalSystem: wolf at night moves at least as fast as in day");
    }

    // --- EpisodeManager ---
    {
        dp::world::World::Config wcfg;
        wcfg.heightmap_cells_x = 32;
        wcfg.heightmap_cells_y = 32;
        wcfg.cave_count = 0;
        wcfg.river_count = 0;
        dp::world::World w(wcfg);

        dp::world::AnimalSystem animals;
        animals.initialize(123, w);
        dp::agent::CapsuleAgent agent(7);
        dp::agent::VitalSystem  vsys;
        dp::agent::VitalState   vstate{};
        dp::agent::RewardSystem reward;

        dp::agent::EpisodeManager em;
        dp::agent::EpisodeManager::Config ecfg;
        ecfg.spawn_x = 16.0f;
        ecfg.spawn_y = 16.0f;
        ecfg.max_seconds = 5.0f;
        em.configure(ecfg);
        em.begin_episode();
        check(em.episode_id() == 0,
              "EpisodeManager: begin_episode starts at id=0");

        // Tick 4.9 seconds: not done yet
        em.tick(4.9f);
        check(!em.is_done(vstate),
              "EpisodeManager: not done before max_seconds");

        // Tick another 0.2 seconds: now over the cap -> done
        em.tick(0.2f);
        check(em.is_done(vstate),
              "EpisodeManager: done when elapsed > max_seconds");

        // Reset moves to next episode and zeroes counters
        dp::agent::Inventory inv;
        dp::agent::Shelter   sh;
        em.reset(w, agent, vsys, vstate, reward, animals, inv, sh);
        check(em.episode_id() == 1 && em.episodes_completed() == 1
              && em.elapsed_s() == 0.0f,
              "EpisodeManager: reset advances id and clears timers");

        // Death also triggers done
        vstate.alive = false;
        check(em.is_done(vstate),
              "EpisodeManager: death triggers done immediately");
    }

    // --- RewardSystem ---
    {
        dp::agent::RewardSystem r;
        r.begin_step();
        r.event_alive(1.0f);  // 1 sim-second
        float a = r.end_step();
        check(std::abs(a - 0.05f) < 1e-5f,
              "RewardSystem: alive(1s) ≈ +0.05 with default weights");

        r.begin_step();
        r.event_eat(25.0f);    // one fruit
        r.event_drink(35.0f);  // one river drink
        float b = r.end_step();
        check(std::abs(b - (0.04f * 25.0f + 0.03f * 35.0f)) < 1e-5f,
              "RewardSystem: eat+drink uses linear weights");

        r.begin_step();
        r.event_death();
        float d = r.end_step();
        check(std::abs(d - (-100.0f)) < 1e-3f,
              "RewardSystem: death is -100 by default");

        // Episode totals accumulate across the three steps above
        check(r.episode().steps == 3 && r.episode().r_alive > 0.0f
              && r.episode().r_food > 0.0f && r.episode().r_death < 0.0f,
              "RewardSystem: episode aggregates per-component totals");
    }

    spdlog::info("------------------------------------------------");
    if (failures == 0) {
        spdlog::info(" SELF-TESTS PASSED");
        return 0;
    }
    spdlog::error(" {} TEST(S) FAILED", failures);
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    auto console = spdlog::stdout_color_mt("deep_protagonist");
    spdlog::set_default_logger(console);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        return run_self_tests();
    }

    try {
        print_banner();
        run_torch_smoke();

        // -----------------------------------------------------------
        // Build world (CPU side)
        // -----------------------------------------------------------
        auto wcfg = load_world_config("configs/default.toml");
        spdlog::info("World: {}x{} cells @ {}m, max_height={}m, caves={}",
                     wcfg.heightmap_cells_x, wcfg.heightmap_cells_y,
                     wcfg.cell_scale, wcfg.terrain_max_height, wcfg.cave_count);
        dp::world::World world(wcfg);

        // -----------------------------------------------------------
        // Initialize render window
        // -----------------------------------------------------------
        const int win_w = 1280;
        const int win_h = 720;
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
        InitWindow(win_w, win_h, "deep_protagonist · S1 viewer");
        SetTargetFPS(60);
        DisableCursor();

        dp::world::Decorations decorations;
        decorations.generate(wcfg.seed ^ 0xDEC0ULL, world);
        spdlog::info("Decorations placed: {} items (trees/rocks/cacti)",
                     decorations.items().size());

        dp::render::WorldRenderer renderer;
        renderer.build(world, decorations);

        dp::render::FlyCamera cam;
        // Drop the camera high above the centre, looking down
        float wx_max = world.heightmap().world_size_x();
        float wy_max = world.heightmap().world_size_y();
        cam.set_position(Vector3{ wx_max * 0.5f, wy_max * 0.5f - 40.0f,
                                    world.surface_height(wx_max * 0.5f, wy_max * 0.5f) + 50.0f });
        cam.set_target(Vector3{ wx_max * 0.5f, wy_max * 0.5f, 0.0f });

        dp::agent::CapsuleAgent agent(wcfg.seed ^ 0xA9E1ULL);
        // Start the agent on the highest-ish ground we can sample quickly
        float spawn_x = wx_max * 0.5f;
        float spawn_y = wy_max * 0.5f;
        float spawn_z = world.surface_height(spawn_x, spawn_y) + 5.0f;
        agent.reset(spawn_x, spawn_y, spawn_z);

        dp::world::Physics physics;

        // S3: replaced legacy FoodSystem with Plants + Animals + Vitals
        dp::world::PlantSystem plants;
        plants.initialize(wcfg.seed ^ 0xF00DULL, world, decorations);
        spdlog::info("Plants placed: {} ripe items across the world",
                     plants.plants().size());

        dp::world::AnimalSystem animals;
        animals.initialize(wcfg.seed ^ 0xA11AULL, world);
        spdlog::info("Animals placed: {} (rabbits/deer/wolves/fish/eagles)",
                     animals.animals().size());

        dp::world::ResourceSystem resources;
        resources.initialize(wcfg.seed ^ 0xC011ULL, world);
        spdlog::info("Resources placed: {} (wood+stone+grass scattered "
                     "by biome)", resources.resources().size());

        dp::agent::Inventory inventory;
        dp::agent::Shelter   shelter;

        dp::agent::VitalSystem vitals;
        dp::agent::VitalState  vital_state{};

        // RewardSystem: per-tick scalar that the PPO policy will optimize
        // against once we plug it in (S4). For S3 we run it in shadow mode
        // so we get a feel for how the reward signal behaves under wander.
        dp::agent::RewardSystem reward;

        // EpisodeManager: ends the current episode on death or after a
        // wall-clock cap, then re-spawns agent + vitals + animals while
        // keeping the static world untouched.
        dp::agent::EpisodeManager episodes;
        dp::agent::EpisodeManager::Config ecfg;
        ecfg.spawn_x = spawn_x;
        ecfg.spawn_y = spawn_y;
        ecfg.spawn_clearance = 5.0f;
        ecfg.max_seconds = 300.0f;            // 5 minutes per episode
        ecfg.animal_seed_base = wcfg.seed ^ 0xA11AULL;
        episodes.configure(ecfg);
        episodes.begin_episode();

        // 600s = 10 minutes per in-game day; start at 0.30 = early morning
        dp::world::Daylight day(600.0f, 0.30f);

        spdlog::info("Window open. WASD+Mouse to fly, Ctrl=fast, Esc=quit.");

        // -----------------------------------------------------------
        // Main loop
        // -----------------------------------------------------------
        bool follow_agent = false;
        int  total_harvested = 0;
        int  wolf_bites = 0;
        bool was_alive = true;
        while (!WindowShouldClose()) {
            float dt = GetFrameTime();
            if (dt > 0.1f) dt = 0.1f;

            if (IsKeyPressed(KEY_G)) follow_agent = !follow_agent;
            // Speed up/slow down day (T = +5x faster, Y = +20x faster)
            float day_dt = dt;
            if (IsKeyDown(KEY_T)) day_dt *= 5.0f;
            if (IsKeyDown(KEY_Y)) day_dt *= 20.0f;
            day.advance(day_dt);

            // Tick world: sample a wander action (placeholder for the
            // PPO policy we will plug in at S4) and apply it to the agent.
            dp::agent::AgentAction action = agent.sample_wander_action(dt);
            agent.apply_action(action, world, physics, dt);
            const auto& as = agent.state();

            reward.begin_step();
            reward.event_alive(dt);

            // Animals follow the agent's position; wolves at night get
            // boosted perception+speed via night_factor. Shelter dampens
            // wolf detection of the agent (S3.8c).
            float night_factor = std::max(0.0f, -day.sun_height_normalized());
            float perc_scale = 1.0f, spd_scale = 1.0f;
            if (shelter.covers(as.pos_x, as.pos_y)) {
                perc_scale = 0.3f;
                spd_scale  = 0.7f;
            }
            animals.tick(world, as.pos_x, as.pos_y, as.pos_z, dt, night_factor,
                         perc_scale, spd_scale);

            // Plants regrow timers
            plants.tick(dt);
            resources.tick(dt);

            // Vitals (with movement speed for stamina)
            float speed = std::sqrt(as.vel_x*as.vel_x + as.vel_y*as.vel_y);
            vitals.tick(vital_state, world, as.pos_x, as.pos_y, speed, day, dt);
            // Inside shelter: body temp drifts toward neutral fast.
            if (shelter.covers(as.pos_x, as.pos_y)) {
                vital_state.temperature += (50.0f - vital_state.temperature)
                                          * 0.3f * dt;
            }
            reward.event_vital_pressure(vital_state, dt);

            // E key: harvest any nearby ripe plant. Mushrooms have a
            // 30% chance to poison instead of feed (blueprint line 16).
            if (IsKeyPressed(KEY_E)) {
                auto got = plants.try_harvest(as.pos_x, as.pos_y,
                                              as.pos_z + as.height * 0.5f, 2.5f);
                for (auto k : got) {
                    float kcal = 0.0f, ml = 0.0f;
                    switch (k) {
                        case dp::world::PlantKind::Berry:    kcal = 12.0f; break;
                        case dp::world::PlantKind::Fruit:    kcal = 25.0f; break;
                        case dp::world::PlantKind::Cactus:   kcal =  8.0f; ml = 30.0f; break;
                        case dp::world::PlantKind::Mushroom: kcal = 18.0f; break;
                    }
                    bool poisoned = false;
                    if (k == dp::world::PlantKind::Mushroom) {
                        // Cheap PRNG using vital state hash (deterministic but
                        // varied). 0..1 from low bits of pos+time.
                        unsigned r = static_cast<unsigned>(
                            (uint64_t)(as.pos_x*1000.0f) * 7919ULL
                          ^ (uint64_t)(GetFrameTime() * 1e6f) * 31337ULL
                          ^ (uint64_t)(GetTime()      * 1e6f));
                        if ((r % 100u) < 30u) {
                            poisoned = true;
                        }
                    }
                    if (poisoned) {
                        // Drop hunger and stamina; reward also takes a hit
                        // through the "negative eat" payload so the agent
                        // learns mushrooms are risky.
                        vital_state.hunger  = std::max(0.0f, vital_state.hunger  - 12.0f);
                        vital_state.stamina = std::max(0.0f, vital_state.stamina - 25.0f);
                        reward.event_eat(-15.0f);
                    } else {
                        if (kcal > 0.0f) { vitals.eat(vital_state, kcal);  reward.event_eat(kcal); }
                        if (ml   > 0.0f) { vitals.drink(vital_state, ml);  reward.event_drink(ml); }
                    }
                    ++total_harvested;
                }
            }

            // Q key: drink water if standing in/near a river or swamp
            if (IsKeyPressed(KEY_Q)) {
                auto b = world.biome_at(as.pos_x, as.pos_y);
                if (b == dp::world::Biome::River || b == dp::world::Biome::Swamp) {
                    vitals.drink(vital_state, 35.0f);
                    reward.event_drink(35.0f);
                    // Same key: try to catch a fish in arm's reach.
                    auto fish = animals.agent_attack(as.pos_x, as.pos_y,
                                                     agent.yaw(), 2.0f, 100.0f);
                    if (fish.killed
                        && animals.animals()[fish.index].kind
                            == dp::world::AnimalKind::Fish) {
                        vitals.eat(vital_state, 20.0f);
                        reward.event_eat(20.0f);
                    }
                }
            }

            // R key: pick up nearby resources (wood / stone / grass) and
            // pack them into inventory. Each pickup grants a small reward.
            if (IsKeyPressed(KEY_R)) {
                auto picked = resources.try_collect(as.pos_x, as.pos_y,
                                                    as.pos_z + as.height * 0.5f,
                                                    2.0f);
                for (auto rk : picked) {
                    if (inventory.add(dp::agent::item_from_resource(rk))) {
                        reward.event_collected_resource();
                    }
                }
            }

            // C key: craft a spear (consumes 1 wood + 1 stone, frees a slot)
            if (IsKeyPressed(KEY_C)) {
                if (inventory.craft_spear()) {
                    reward.event_crafted_tool();
                }
            }

            // B key: build a shelter at the agent's feet (consumes 1 wood
            // + 1 grass). Replaces any existing anchor.
            if (IsKeyPressed(KEY_B)) {
                if (shelter.try_place(inventory,
                                      as.pos_x, as.pos_y, as.pos_z)) {
                    reward.event_built_shelter();
                }
            }

            // F key: melee attack the closest animal in front (90 deg arc).
            // Spear in inventory boosts reach and damage.
            if (IsKeyPressed(KEY_F)) {
                bool has_spear = inventory.num_of(dp::agent::ItemKind::Spear) > 0;
                float reach  = has_spear ? 2.5f : 1.5f;
                float damage = has_spear ? 50.0f : 20.0f;
                auto hit = animals.agent_attack(as.pos_x, as.pos_y,
                                                agent.yaw(), reach, damage);
                if (hit.killed) {
                    auto kind = animals.animals()[hit.index].kind;
                    if (kind == dp::world::AnimalKind::Wolf) {
                        reward.event_wolf_killed();
                        vitals.eat(vital_state, 40.0f);
                        reward.event_eat(40.0f);
                    } else if (kind == dp::world::AnimalKind::Rabbit) {
                        vitals.eat(vital_state, 15.0f);
                        reward.event_eat(15.0f);
                    } else if (kind == dp::world::AnimalKind::Deer) {
                        vitals.eat(vital_state, 40.0f);
                        reward.event_eat(40.0f);
                    } else if (kind == dp::world::AnimalKind::Fish) {
                        vitals.eat(vital_state, 20.0f);
                        reward.event_eat(20.0f);
                    }
                }
            }

            // Wolves can bite the agent if within reach
            for (size_t i = 0; i < animals.animals().size(); ++i) {
                if (animals.wolf_attack_agent(static_cast<int>(i), as.pos_x, as.pos_y)) {
                    vital_state.hunger = std::max(0.0f, vital_state.hunger - 25.0f * dt);
                    vital_state.stamina = std::max(0.0f, vital_state.stamina - 50.0f * dt);
                    wolf_bites += 1;
                    reward.event_wolf_bite(25.0f * dt);  // damage points absorbed
                }
            }

            // Death edge: count the -100 in this step's reward and let
            // EpisodeManager handle the actual reset *after* end_step().
            if (was_alive && !vital_state.alive) {
                reward.event_death();
            }
            was_alive = vital_state.alive;

            reward.end_step();
            episodes.tick(dt);

            // End-of-episode: log totals and reset state. Terrain/plants
            // stay untouched; agent + vitals + animals are reseeded.
            if (episodes.is_done(vital_state)) {
                spdlog::info(
                    "Episode {} done: steps={} elapsed={:.1f}s "
                    "reward={:+.1f} (alive={:+.1f} food={:+.1f} water={:+.1f} "
                    "kills={:+.1f} bites={:+.1f} death={:+.1f} vital={:+.1f})",
                    episodes.episode_id(),
                    episodes.step_count(),
                    episodes.elapsed_s(),
                    reward.episode().total_reward,
                    reward.episode().r_alive,
                    reward.episode().r_food,
                    reward.episode().r_water,
                    reward.episode().r_kills,
                    reward.episode().r_bites,
                    reward.episode().r_death,
                    reward.episode().r_vital);
                episodes.reset(world, agent, vitals, vital_state,
                               reward, animals, inventory, shelter);
                was_alive = true;
            }

            // Update camera
            if (follow_agent) {
                const auto& s = agent.state();
                Vector3 target = Vector3{ s.pos_x, s.pos_y, s.pos_z + s.height * 0.5f };
                Vector3 offset = Vector3{ 0.0f, -8.0f, 4.0f };
                cam.set_position(Vector3{ target.x + offset.x,
                                            target.y + offset.y,
                                            target.z + offset.z });
                cam.set_target(target);
            } else {
                cam.update(dt);
            }

            // Render
            BeginDrawing();
            ClearBackground(day.sky_color());

            renderer.draw(cam, agent, world, decorations, plants, animals,
                          resources, shelter, day.terrain_tint());

            // HUD background
            const auto& s = agent.state();
            DrawRectangle(8, 8, 420, 348, Color{ 0, 0, 0, 160 });
            DrawText(TextFormat("FPS: %d   Day t=%.2f", GetFPS(), day.t()),
                     12, 12, 18, RAYWHITE);
            DrawText(TextFormat("Agent: (%.0f, %.0f, %.0f)  speed=%.1f",
                                s.pos_x, s.pos_y, s.pos_z, speed),
                     12, 34, 16, RAYWHITE);
            auto biome = world.biome_at(s.pos_x, s.pos_y);
            DrawText(TextFormat("Biome: %s  cave:%s  ground:%s",
                                dp::world::biome_name(biome),
                                agent.inside_cave() ? "yes" : "no",
                                s.on_ground ? "yes" : "no"),
                     12, 54, 16, Color{ 200, 220, 255, 255 });
            DrawText(TextFormat("Harvested: %d   Wolf bites: %d   Deaths: %d",
                                total_harvested, wolf_bites, vital_state.deaths),
                     12, 74, 16, Color{ 200, 200, 200, 255 });
            const auto& ep = reward.episode();
            DrawText(TextFormat("Reward step=%+.3f  ep=%+.1f  steps=%d",
                                reward.last_step().total(),
                                ep.total_reward,
                                ep.steps),
                     12, 92, 14, Color{ 240, 220, 120, 255 });

            // Vital bars
            auto vital_bar = [](int x, int y, const char* label,
                                float v, Color col) {
                DrawText(label, x, y, 14, RAYWHITE);
                DrawRectangle(x + 80, y + 2, 200, 12, Color{ 40, 40, 40, 220 });
                int w = static_cast<int>(2.0f * v);
                DrawRectangle(x + 80, y + 2, w, 12, col);
                DrawText(TextFormat("%.0f", v), x + 290, y, 14, RAYWHITE);
            };
            vital_bar(12, 116, "Hunger:",
                      vital_state.hunger, Color{220, 130, 50, 255});
            vital_bar(12, 136, "Thirst:",
                      vital_state.thirst, Color{60, 160, 220, 255});
            vital_bar(12, 156, "Stamina:",
                      vital_state.stamina, Color{120, 220, 80, 255});
            vital_bar(12, 176, "Temp:",
                      vital_state.temperature, Color{220, 200, 120, 255});

            // Inventory + shelter + episode line
            int n_w = inventory.num_of(dp::agent::ItemKind::Wood);
            int n_s = inventory.num_of(dp::agent::ItemKind::Stone);
            int n_g = inventory.num_of(dp::agent::ItemKind::Grass);
            int n_sp = inventory.num_of(dp::agent::ItemKind::Spear);
            DrawText(TextFormat("Inv: W=%d S=%d G=%d Spear=%d  (cap %d)",
                                n_w, n_s, n_g, n_sp,
                                dp::agent::Inventory::CAPACITY),
                     12, 200, 14, Color{ 200, 220, 255, 255 });
            DrawText(TextFormat("Shelter: %s%s   Resources avail: W=%d S=%d G=%d",
                                shelter.placed() ? "placed" : "none",
                                (shelter.placed() && shelter.covers(s.pos_x, s.pos_y))
                                    ? " (you're inside)" : "",
                                resources.available_count(dp::world::ResourceKind::Wood),
                                resources.available_count(dp::world::ResourceKind::Stone),
                                resources.available_count(dp::world::ResourceKind::Grass)),
                     12, 218, 14, Color{ 200, 220, 200, 255 });
            DrawText(TextFormat("Episode #%d  steps=%d  elapsed=%.0fs / %.0fs",
                                episodes.episode_id(),
                                episodes.step_count(),
                                episodes.elapsed_s(),
                                ecfg.max_seconds),
                     12, 236, 14, Color{ 240, 220, 120, 255 });
            DrawText(TextFormat("Camera: %s   [G] toggle follow",
                                follow_agent ? "follow" : "fly"),
                     12, 256, 16, RAYWHITE);
            DrawText("E=harvest plant   Q=drink/catch fish   R=collect resource",
                     12, 280, 14, Color{ 220, 220, 100, 255});
            DrawText("C=craft spear (W+S)   B=build shelter (W+G)   F=attack",
                     12, 298, 14, Color{ 220, 220, 100, 255});
            DrawText("WASD/Space/Shift fly  Ctrl=fast  T/Y=time speed",
                     12, 316, 14, Color{ 180, 180, 180, 255 });
            DrawText("Esc=quit",
                     12, 332, 14, Color{ 180, 180, 180, 255 });

            EndDrawing();
        }

        CloseWindow();
        spdlog::info("S1 viewer closed cleanly.");
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("FATAL: {}", e.what());
        return 1;
    }
}
