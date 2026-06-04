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
#include "agent/ProgressTracker.hpp"
#include "agent/RewardSystem.hpp"
#include "agent/Shelter.hpp"
#include "agent/SpatialMemory.hpp"
#include "agent/TameSystem.hpp"
#include "agent/VitalSystem.hpp"
#include "agent/Wardrobe.hpp"
#include "env/Environment.hpp"
#include "policy/PPO.hpp"
#include "render/FlyCamera.hpp"
#include "render/WorldRenderer.hpp"
#include "world/AnimalSystem.hpp"
#include "world/Daylight.hpp"
#include "world/Decorations.hpp"
#include "world/FoodSystem.hpp"
#include "world/Physics.hpp"
#include "world/BuildingSystem.hpp"
#include "world/FarmingSystem.hpp"
#include "world/PlantSystem.hpp"
#include "world/ResourceSystem.hpp"
#include "world/World.hpp"

#include <torch/torch.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <toml++/toml.hpp>

#include <raylib.h>

#include <algorithm>
#include <array>
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

    // --- decision_blueprint_nutrition (round_21): dual nutrition bars ---
    // Verifies the four required behaviours (掉/回/debuff/死) plus the
    // "disabled by default" guarantee that keeps the old world identical.
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 16;
        cfg.heightmap_cells_y = 16;
        cfg.cave_count  = 0;
        cfg.river_count = 0;
        dp::world::World w(cfg);
        dp::world::Daylight day(60.0f, 0.5f);  // noon -> no night penalty

        // (0) Disabled by default: bars stay full, decay/eat are no-ops.
        {
            dp::agent::VitalSystem vs;          // default cfg: nutrition OFF
            dp::agent::VitalState  v{};
            check(v.protein == 100.0f && v.vitamin == 100.0f,
                  "nutrition: VitalState defaults protein/vitamin = 100");
            for (int i = 0; i < 120; ++i)
                vs.tick(v, w, 30.0f, 30.0f, 0.0f, day, 1.0f);
            check(v.protein == 100.0f && v.vitamin == 100.0f,
                  "nutrition OFF: bars never decay (old world unaffected)");
            check(vs.eat_protein(v, 50.0f) == 0.0f
                  && vs.nutrition_speed_mult(v) == 1.0f,
                  "nutrition OFF: eat_protein no-op & speed_mult = 1.0");
        }

        dp::agent::VitalConfig nc;
        nc.nutrition_enabled     = true;
        nc.protein_decay_per_min = 60.0f;   // 1 bar/min full->empty
        nc.vitamin_decay_per_min = 60.0f;
        nc.nutri_debuff_thresh   = 0.30f;
        nc.nutri_debuff_floor    = 0.5f;
        nc.nutri_death_grace_s   = 2.0f;

        // (1) 掉: bars decay over time.
        {
            dp::agent::VitalSystem vs(nc);
            dp::agent::VitalState  v{};
            for (int i = 0; i < 30; ++i)        // 30s @60/min -> ~-30
                vs.tick(v, w, 30.0f, 30.0f, 0.0f, day, 1.0f);
            check(v.protein < 75.0f && v.protein > 65.0f
               && v.vitamin < 75.0f && v.vitamin > 65.0f,
                  "nutrition: protein/vitamin decay ~30 over 30s @60/min");
        }

        // (2) 回: eating meat/forage refills the matching bar (clamped).
        {
            dp::agent::VitalSystem vs(nc);
            dp::agent::VitalState  v{};
            v.protein = 40.0f; v.vitamin = 40.0f;
            float gotp = vs.eat_protein(v, 35.0f);
            float gotv = vs.eat_vitamin(v, 30.0f);
            check(gotp > 34.0f && v.protein > 74.0f && v.protein < 76.0f
               && v.vitamin == 40.0f + 30.0f,
                  "nutrition: eat_protein refills protein only");
            check(gotv > 29.0f && v.vitamin == 70.0f,
                  "nutrition: eat_vitamin refills vitamin only");
            v.protein = 90.0f;
            vs.eat_protein(v, 50.0f);
            check(v.protein == 100.0f, "nutrition: protein clamps at 100");
        }

        // (3) debuff: a bar below threshold weakens movement.
        {
            dp::agent::VitalSystem vs(nc);
            dp::agent::VitalState  v{};
            check(vs.nutrition_speed_mult(v) == 1.0f,
                  "nutrition: full bars -> speed_mult 1.0 (no debuff)");
            v.protein = 15.0f;                  // below 30% threshold
            float mult_low = vs.nutrition_speed_mult(v);
            check(mult_low < 1.0f && mult_low >= 0.5f,
                  "nutrition: protein<30% -> movement debuff in [floor,1)");
            v.protein = 0.0f; v.vitamin = 0.0f;
            check(std::abs(vs.nutrition_speed_mult(v) - 0.25f) < 1e-4f,
                  "nutrition: both bars empty -> floor*floor = 0.25");
        }

        // (4) 死: a bar held at zero past the grace window kills with the
        //     correct cause counter, distinct from food/cold.
        {
            dp::agent::VitalSystem vs(nc);
            dp::agent::VitalState  v{};
            v.protein = 0.0f;
            int ticks = 0;
            while (v.alive && ticks < 10) {     // grace 2s -> dies by tick 3
                vs.tick(v, w, 30.0f, 30.0f, 0.0f, day, 1.0f);
                ++ticks;
            }
            check(!v.alive && v.deaths_protein == 1 && v.deaths_vitamin == 0
               && v.deaths_food == 0,
                  "nutrition: empty protein past grace -> death_protein");

            dp::agent::VitalState v2{};
            v2.vitamin = 0.0f;
            ticks = 0;
            while (v2.alive && ticks < 10) {
                vs.tick(v2, w, 30.0f, 30.0f, 0.0f, day, 1.0f);
                ++ticks;
            }
            check(!v2.alive && v2.deaths_vitamin == 1 && v2.deaths_protein == 0,
                  "nutrition: empty vitamin past grace -> death_vitamin");
        }
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

    // --- ProgressTracker ---
    {
        dp::agent::ProgressTracker p;
        check(p.stage() == dp::agent::ProgressStage::Early,
              "ProgressTracker: starts in Early stage");
        check(p.can_build() == false && p.can_farm() == false,
              "ProgressTracker: Early stage gates build/farm");

        // Not enough to advance yet (use threshold-relative values so this test
        // stays correct if mid_collect_threshold / mid_explore_threshold change.
        // D-086 dropped thresholds 30/500 -> 5/50, prior hard-coded 15/200 broke.)
        const int   below_collect = (p.mid_collect_threshold > 1) ? (p.mid_collect_threshold - 1) : 0;
        const float below_explore = (p.mid_explore_threshold > 1.0f) ? (p.mid_explore_threshold - 1.0f) : 0.0f;
        p.on_collect(below_collect);
        p.on_explore_step(below_explore);
        check(p.stage() == dp::agent::ProgressStage::Early,
              "ProgressTracker: stays Early below thresholds");
        check(p.poll_unlock() == false,
              "ProgressTracker: no unlock event yet");

        // Cross both thresholds -> Mid
        p.on_collect(p.mid_collect_threshold + 2);   // bumps total above
        p.on_explore_step(p.mid_explore_threshold + 2.0f);  // bumps total above
        check(p.stage() == dp::agent::ProgressStage::Mid,
              "ProgressTracker: advances to Mid after thresholds");
        check(p.poll_unlock() == true,
              "ProgressTracker: unlock event fires on advance");
        check(p.poll_unlock() == false,
              "ProgressTracker: unlock event is single-shot");
        check(p.can_build() == true,
              "ProgressTracker: Mid stage enables build");

        // Build a house -> Late
        p.on_house_built();
        check(p.stage() == dp::agent::ProgressStage::Late,
              "ProgressTracker: Late stage after first house");
        check(p.can_farm() && p.can_tame(),
              "ProgressTracker: Late stage enables farm + tame");
    }

    // --- BuildingSystem ---
    {
        dp::world::BuildingSystem bs;
        dp::agent::ProgressTracker prog;

        // Early stage cannot place
        int idx = bs.place_site(dp::world::BuildingType::Shed, 0, 0, 0, prog);
        check(idx < 0, "BuildingSystem: Early stage cannot place site");

        // Force progression to Mid
        prog.on_collect(prog.mid_collect_threshold);
        prog.on_explore_step(prog.mid_explore_threshold + 1.0f);
        idx = bs.place_site(dp::world::BuildingType::Shed, 5, 5, 0, prog);
        check(idx >= 0, "BuildingSystem: Mid stage places site");
        check(bs.sites().size() == 1, "BuildingSystem: site count=1");

        // D-055: blueprint line 30 single-site rule. While the first site
        // is incomplete, a second place_blueprint must be refused.
        int reject_idx = bs.place_site(dp::world::BuildingType::WoodHouse,
                                       50, 50, 0, prog);
        check(reject_idx < 0,
              "BuildingSystem: second place rejected while site incomplete (D-055)");
        check(bs.sites().size() == 1,
              "BuildingSystem: rejected place adds no site (D-055)");

        // Deposit materials until ready
        const auto& r = dp::world::building_recipe(dp::world::BuildingType::Shed);
        for (int i = 0; i < r.wood_required; ++i) {
            bs.try_deposit(5, 5, 0, 2.0f, dp::agent::ItemKind::Wood);
        }
        for (int i = 0; i < r.grass_required; ++i) {
            bs.try_deposit(5, 5, 0, 2.0f, dp::agent::ItemKind::Grass);
        }
        check(bs.sites()[0].ready(),
              "BuildingSystem: site ready after all materials");
        int finalized = bs.tick_finalize();
        check(finalized == 1 && bs.buildings().size() == 1,
              "BuildingSystem: tick_finalize promotes ready site");
        check(bs.covers(5, 5) == true,
              "BuildingSystem: completed building covers its anchor");

        // D-055: once the first site is *completed*, a new place_blueprint
        // is allowed again (single-active-site rule, not single-ever).
        int third_idx = bs.place_site(dp::world::BuildingType::Shed,
                                      80, 80, 0, prog);
        check(third_idx >= 0,
              "BuildingSystem: place allowed after previous site completes (D-055)");
        check(bs.sites().size() == 2,
              "BuildingSystem: new site appended after completion (D-055)");
    }

    // --- Wardrobe + clothing craft ---
    {
        dp::agent::Inventory inv;
        for (int i = 0; i < 3; ++i) inv.add(dp::agent::ItemKind::Grass);
        check(inv.craft_grass_dress(),
              "Wardrobe: 3 grass crafts a grass dress");
        check(inv.num_of(dp::agent::ItemKind::GrassDress) == 1
              && inv.num_of(dp::agent::ItemKind::Grass) == 0,
              "Wardrobe: craft consumes grass adds dress");

        dp::agent::Wardrobe w;
        check(w.cold_resist() == 0.0f, "Wardrobe: no cold resist when naked");
        check(w.wear(inv, dp::agent::ItemKind::GrassDress),
              "Wardrobe: wear grass dress succeeds");
        check(w.cold_resist() > 0.05f,
              "Wardrobe: cold resist > 0 with grass dress");

        // Fur cloak: need 2 fur
        inv.clear();
        inv.add(dp::agent::ItemKind::Fur);
        inv.add(dp::agent::ItemKind::Fur);
        check(inv.craft_fur_cloak(),
              "Wardrobe: 2 fur crafts a cloak");
        check(w.wear(inv, dp::agent::ItemKind::FurCloak),
              "Wardrobe: wear cloak (replaces dress)");
        check(w.cold_resist() > 0.20f,
              "Wardrobe: cloak gives stronger cold resist");
    }

    // --- FarmingSystem ---
    {
        dp::world::FarmingSystem f;
        int p = f.plant_seed(10, 10, 0, dp::world::PlantKind::Berry);
        check(p == 0 && f.total_count() == 1,
              "FarmingSystem: plant_seed creates a plot");

        // Plot starts dry-ish; water periodically and tick should grow it
        // (water drains faster than the plot grows, so re-water often).
        for (int i = 0; i < 200; ++i) {
            if ((i % 10) == 0) f.water_at(10, 10);
            f.tick(1.0f);
        }
        check(f.ripe_count() >= 1,
              "FarmingSystem: plot ripens with periodic watering");

        // Harvest
        auto got = f.harvest_at(10, 10);
        check(got.size() == 1 && got[0] == dp::world::PlantKind::Berry,
              "FarmingSystem: harvest returns the seed kind");
        f.tick(0.1f);  // disposes harvested plots
        check(f.total_count() == 0,
              "FarmingSystem: harvested plot removed after tick");
    }

    // --- TameSystem ---
    {
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 32;
        cfg.heightmap_cells_y = 32;
        cfg.cave_count = 0;
        cfg.river_count = 0;
        dp::world::World w(cfg);

        dp::world::AnimalSystem animals;
        animals.initialize(77, w, 1, 0, 0, 0, 0);  // 1 rabbit only
        const auto& a0 = animals.animals()[0];

        dp::agent::TameSystem t;
        dp::agent::Inventory inv;
        // No food -> feed fails
        auto fr0 = t.try_feed(animals, inv, a0.pos_x, a0.pos_y);
        check(fr0.animal_idx < 0,
              "TameSystem: feed with empty inventory misses");

        // Stuff inventory with grass and feed enough to become follower
        for (int i = 0; i < 3; ++i) inv.add(dp::agent::ItemKind::Grass);
        bool became_follower = false;
        for (int i = 0; i < 10; ++i) {
            auto fr = t.try_feed(animals, inv, a0.pos_x, a0.pos_y);
            if (fr.animal_idx >= 0) inv.remove_one(fr.consumed);
            if (fr.just_became_follower) became_follower = true;
            // refill grass so we don't run dry
            if (inv.num_of(dp::agent::ItemKind::Grass) == 0)
                inv.add(dp::agent::ItemKind::Grass);
        }
        check(became_follower,
              "TameSystem: rabbit becomes follower after enough grass feeds");
        check(t.follower_count() == 1,
              "TameSystem: follower count tracks correctly");

        // Follower-vs-wolf protection (S3.10 polish C):
        // Tag the rabbit as tamed, place a wolf right next to it, and
        // verify the wolf doesn't pick it as prey (we just check the
        // rabbit's is_tamed flag round-trips and tick_wolf doesn't kill).
        dp::world::AnimalSystem ans2;
        ans2.initialize(78, w, 1, 0, 1, 0, 0);  // 1 rabbit + 1 wolf
        ans2.mark_tamed(0, true);
        // Spawn wolf right next to rabbit by stepping a few frames with
        // the wolf perception pointing at the rabbit.
        const auto& rabbit_init = ans2.animals()[0];
        const auto& wolf_init   = ans2.animals()[1];
        (void)wolf_init;
        for (int i = 0; i < 30; ++i) {
            ans2.tick(w, rabbit_init.pos_x + 60.0f,
                      rabbit_init.pos_y + 60.0f, 0.0f, 0.1f);
        }
        check(ans2.animals()[0].state != dp::world::AnimalState::Dead,
              "TameSystem: tamed follower is not killed by nearby wolf");
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
        dp::agent::ProgressTracker  progress;
        dp::world::BuildingSystem   buildings;
        dp::agent::Wardrobe         wardrobe;
        dp::world::FarmingSystem    farming;
        dp::agent::TameSystem       tame;

        dp::agent::SpatialMemory sm_test;
        sm_test.configure(0.0f, 0.0f);

        dp::agent::ObservationBuilder::Vector obs{};
        dp::agent::ObservationBuilder::compose(
            w, agent, vstate, day, plants, animals, resources,
            inv, sh, em, progress, buildings, wardrobe, farming, tame,
            dp::world::BuildingType::Shed, sm_test, 0.0f, 0.0f, obs);

        check(dp::agent::ObservationBuilder::SIZE >= 140,
              "Observation: total dimension is at least 140 (S3.10 expanded)");
        // D-048: 564 -> 567 (nearest_wolf). nutrition §3a: 567 -> 569
        // (protein, vitamin). S5 world realism: 569 -> 571 (ambient_temp,
        // slope). Stage W1 weather: 571 -> 573 (rain_intensity, wetness).
        // D-122 one-pot tech-tree: 573 -> 582 (+9 reserved tech-tree slots).
        check(dp::agent::ObservationBuilder::SIZE == 582,
              "Observation: SIZE == 582 after D-122 tech-tree reserve (573+9)");
        check(obs[0] == 1.0f && obs[1] == 1.0f && obs[2] == 1.0f,
              "Observation: vital slots [0..2] are 1.0 at full state");
        // nutrition bars live at 567,568; full bars => 1.0/1.0 (constant when
        // nutrition OFF so a warm-started net is blind to them via surgery).
        check(obs[567] == 1.0f && obs[568] == 1.0f,
              "Observation: nutrition slots [567,568] are 1.0 at full bars");
        // Stage W1 weather slots live at the very tail (571 rain_intensity,
        // 572 wetness); both 0 when weather is OFF / dry, so the surgery's
        // zero columns leave a warm-started net blind to them.
        check(obs[571] == 0.0f && obs[572] == 0.0f,
              "Observation: weather slots [571,572] are 0 with weather off");

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
            inv, sh, em, progress, buildings, wardrobe, farming, tame,
            dp::world::BuildingType::Shed, sm_test, 0.0f, 0.0f, obs2);
        // Sanity check: the shelter "placed" slot sits 3 slots before the
        // S3.10 progress block, then 38 dims of P0+D-048 obs, then 384 dims
        // of SpatialMemory window, then 6 tail scalars appended at the very
        // end: 2 nutrition (§3a) + 2 S5 ambient_temp/slope + 2 W1 weather.
        // So index = SIZE - 6 - 384 - 38 - 21 - 3 (= 121, unchanged layout).
        int placed_idx = dp::agent::ObservationBuilder::SIZE
                       - 6  // nutrition(2)+ambient/slope(2)+weather(2) tail
                       - dp::agent::SpatialMemory::WINDOW_DIM - 38 - 21 - 3;
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

    // --- Biome adaptation: desert thirst & swamp speed ---
    {
        // We need an actual 1024m world to find desert/swamp cells; the
        // 16x16 scratch world has no biome variety.
        // Use a seed that produces variety in biomes including desert.
        dp::world::World::Config cfg;
        cfg.heightmap_cells_x = 256;
        cfg.heightmap_cells_y = 256;
        cfg.cave_count = 0;
        cfg.river_count = 0;
        cfg.seed = 0xDEADBEEFULL;
        dp::world::World w(cfg);

        // Find a desert cell if any; otherwise just exercise the path.
        bool found_desert = false;
        float dx = 0.0f, dy = 0.0f;
        float wx_max = w.heightmap().world_size_x();
        float wy_max = w.heightmap().world_size_y();
        int counts[6] = {0,0,0,0,0,0};
        for (int j = 0; j < 64; ++j) {
            for (int i = 0; i < 64; ++i) {
                float x = wx_max * (i + 0.5f) / 64.0f;
                float y = wy_max * (j + 0.5f) / 64.0f;
                auto b = w.biome_at(x, y);
                int bi = static_cast<int>(b);
                if (bi >= 0 && bi < 6) counts[bi]++;
                if (b == dp::world::Biome::Desert && !found_desert) {
                    dx = x; dy = y; found_desert = true;
                }
            }
        }

        // Even if no desert, we still want a smoke check that the new code
        // path doesn't crash.
        dp::agent::VitalSystem vs;
        dp::agent::VitalState v{};
        dp::world::Daylight day(60.0f, 0.5f);  // noon
        for (int i = 0; i < 60; ++i) {
            vs.tick(v, w, dx, dy, 0.0f, day, 1.0f);
        }
        if (found_desert) {
            // 60s of noon-desert standing: thirst should drop ~2x faster
            // than a non-desert site would.
            check(v.thirst < 96.0f,
                  "Desert noon: thirst loss accelerated (sun_h>0.4)");
        } else {
            check(v.thirst <= 100.0f,
                  "Biome path runs without crashing even if no desert");
        }

        // Swamp speed: build agent on a swamp-found cell if any
        bool found_swamp = false;
        float sx = 0.0f, sy = 0.0f;
        for (int j = 0; j < 32 && !found_swamp; ++j) {
            for (int i = 0; i < 32 && !found_swamp; ++i) {
                float x = wx_max * (i + 0.5f) / 32.0f;
                float y = wy_max * (j + 0.5f) / 32.0f;
                if (w.biome_at(x, y) == dp::world::Biome::Swamp) {
                    sx = x; sy = y; found_swamp = true;
                }
            }
        }
        if (found_swamp) {
            dp::world::Physics phys;
            dp::agent::CapsuleAgent agent(99);
            agent.reset(sx, sy, w.surface_height(sx, sy) + 1.0f);
            // Drop to ground
            for (int i = 0; i < 60; ++i) {
                agent.apply_action(dp::agent::noop_action(), w, phys, 1.0f / 60.0f);
            }
            dp::agent::AgentAction east{};
            east.move_x = 1.0f;
            agent.apply_action(east, w, phys, 1.0f / 60.0f);
            // Swamp halves speed: vel_x should be 1.5 m/s (3.0 base * 0.5)
            check(agent.state().vel_x < 2.0f,
                  "Swamp halves agent move speed");
        }
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
        dp::world::BuildingSystem bs;
        em.reset(w, agent, vsys, vstate, reward, animals, inv, sh, bs);
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

        // D-043: eat/drink now use drive_reduction semantics (Keramati 2014).
        // Callers pass drive_before-drive_after ∈ [0,1] instead of kcal/ml.
        r.begin_step();
        r.event_eat(0.5f);    // half-recovery from depletion
        r.event_drink(0.3f);
        float b = r.end_step();
        // D-048 blueprint回归: food_coef/water_coef 3 → 6 to make the
        // drink/eat gradient strong enough that PPO actually pursues
        // water/food instead of settling on the cheaper sleep trigger.
        check(std::abs(b - (6.0f * 0.5f + 6.0f * 0.3f)) < 1e-5f,
              "RewardSystem: eat+drink uses drive-reduction × coef (D-048: ×6)");

        r.begin_step();
        r.event_death();
        float d = r.end_step();
        // D-043: death magnitude reduced from -100 to -10 (Crafter-aligned).
        // Clip at reward_clip_min = -10 keeps the signal intact.
        check(std::abs(d - (-10.0f)) < 1e-3f,
              "RewardSystem: death is -10 by default (D-043)");

        // Episode totals accumulate across the three steps above
        check(r.episode().steps == 3 && r.episode().r_alive > 0.0f
              && r.episode().r_food > 0.0f && r.episode().r_death < 0.0f,
              "RewardSystem: episode aggregates per-component totals");

        // D-043: milestone event is +1 (Crafter-aligned), scale param damps it.
        r.reset_episode();
        r.begin_step();
        r.event_milestone();
        float m = r.end_step();
        check(std::abs(m - 1.0f) < 1e-3f,
              "RewardSystem: milestone event is +1 by default (D-043)");

        r.reset_episode();
        r.begin_step();
        r.event_milestone(0.3f);  // crisis-scale damping
        float m_crisis = r.end_step();
        check(std::abs(m_crisis - 0.3f) < 1e-3f,
              "RewardSystem: milestone honors crisis scale param");

        // D-047: wolf_killed reward zeroed. Killing a wolf no longer pays
        // the +1 carrot that drove 撩狼 collapse in d046_2M (after clip
        // capped the bite-cluster cost, attack-wolf became positive EV).
        r.reset_episode();
        r.begin_step();
        r.event_wolf_killed();
        float wk = r.end_step();
        check(std::abs(wk) < 1e-5f,
              "RewardSystem: wolf_killed contributes 0 reward (D-047)");

        // D-044: end_step return value is CLIPPED to [-10, +10]. Pre D-044
        // the caller (Environment::apply_action_effects_) discarded this
        // return value and StepResult.reward leaked the unclipped sum,
        // so PPO never saw the D-043 P1 safety clip.
        r.reset_episode();
        r.begin_step();
        r.event_wolf_bite(50.0f);  // -50 raw, should clip to -10
        float clip_lo = r.end_step();
        check(std::abs(clip_lo - (-10.0f)) < 1e-3f,
              "RewardSystem: end_step clips to reward_clip_min (D-044)");
        // The component value still records the unclipped intent.
        check(std::abs(r.last_step().bites - (-50.0f)) < 1e-3f,
              "RewardSystem: clip preserves unclipped components for debug");

        // D-122 credit-assignment: convex per-vital danger penalty. Verifies it
        // (a) is ~0 when all vitals are comfortable, and (b) concentrates blame
        // on a single neglected vital -> a lone critical vital yields a far
        // larger penalty than the SAME total linear deficit spread thin.
        auto mk_vital = [](float h, float t, float st, float temp,
                           float pr, float vi) {
            dp::agent::VitalState v;
            v.hunger = h; v.thirst = t; v.stamina = st;
            v.temperature = temp; v.protein = pr; v.vitamin = vi;
            v.alive = true;
            return v;
        };
        // (a) all comfortable (100, temp neutral 50) -> danger ~ 0.
        r.reset_episode();
        r.begin_step();
        r.event_vital_danger(mk_vital(100, 100, 100, 50, 100, 100), 1.0f);
        r.end_step();   // commit so last_step() reflects this step
        float danger_full = r.last_step().vital;
        check(std::abs(danger_full) < 1e-4f,
              "RewardSystem: vital_danger ~0 when all vitals comfortable");
        // (b1) one vital cratered (thirst=10), rest full. Linear deficit = 90.
        r.reset_episode();
        r.begin_step();
        r.event_vital_danger(mk_vital(100, 10, 100, 50, 100, 100), 1.0f);
        r.end_step();
        float danger_crit = r.last_step().vital;   // ~ -0.02*0.81 = -0.0162
        // (b2) same TOTAL linear deficit (90) spread across 5 vitals (each 82).
        r.reset_episode();
        r.begin_step();
        r.event_vital_danger(mk_vital(82, 82, 82, 50, 82, 82), 1.0f);
        r.end_step();
        float danger_spread = r.last_step().vital;  // ~ -0.02*0.162 = -0.0032
        check(danger_crit < 0.0f && danger_spread < 0.0f
              && danger_crit < danger_spread - 0.005f,
              "RewardSystem: vital_danger concentrates blame on the neglected "
              "vital (one critical >> same linear deficit spread thin)");

        r.reset_episode();
        r.begin_step();
        r.event_milestone(1.0f);
        r.event_milestone(1.0f);
        r.event_milestone(1.0f);
        r.event_milestone(1.0f);
        r.event_milestone(1.0f);
        r.event_milestone(1.0f);
        r.event_milestone(1.0f);
        r.event_milestone(1.0f);
        r.event_milestone(1.0f);
        r.event_milestone(1.0f);
        r.event_milestone(1.0f);  // 11 × +1 = +11 raw
        float clip_hi = r.end_step();
        check(std::abs(clip_hi - 10.0f) < 1e-3f,
              "RewardSystem: end_step clips to reward_clip_max (D-044)");

        // D-043 P2: per-step state-based bonus = state_bonus_coef × phi.
        r.reset_episode();
        r.begin_step();
        r.event_state_bonus(1.5f);  // phi=1.5 (max realistic)
        float sb = r.end_step();
        check(std::abs(sb - (0.005f * 1.5f)) < 1e-5f,
              "RewardSystem: state_bonus = coef × phi (D-043 P2)");

        // D-055: deposit gets its own reward path (+1.0 by default vs
        // collect +0.5). Both fold into step_.collect so the jsonl schema
        // stays at 11 components, but the magnitudes differ so PPO can
        // see "deposit > collect" gradient.
        r.reset_episode();
        r.begin_step();
        r.event_collected_resource();
        float c_only = r.end_step();
        check(std::abs(c_only - 0.5f) < 1e-5f,
              "RewardSystem: collect alone is +0.5 (D-055 baseline)");
        r.reset_episode();
        r.begin_step();
        r.event_deposited_material();
        float d_only = r.end_step();
        check(std::abs(d_only - 1.0f) < 1e-5f,
              "RewardSystem: deposit alone is +1.0 (D-055)");
        check(d_only > c_only + 1e-5f,
              "RewardSystem: deposit reward > collect reward (D-055)");

        // D-059: rolled back to the original 5 (D-056/D-057's bigger
        // shelter bonus was unsuccessful - any reward landscape change
        // on top of D-055 ckpt collapsed PPO into a do-nothing policy).
        r.reset_episode();
        r.begin_step();
        r.event_built_shelter();
        float bs = r.end_step();
        check(std::abs(bs - 5.0f) < 1e-3f,
              "RewardSystem: built_shelter restored to +5 (D-059)");

        // D-062: a 5x milestone stack (used by Environment's
        // fire_once_critical for first_eat / first_drink / first_spear /
        // first_clothing / first_hunt) should land at exactly +5 with
        // crisis_scale=1.0. This is the magnitude PPO will see the very
        // first time it succeeds in a hunt or wears clothes inside an
        // episode; everything after that is the normal +1 milestone path
        // (or the per-event drive / collect rewards).
        r.reset_episode();
        r.begin_step();
        for (int i = 0; i < 5; ++i) r.event_milestone(1.0f);
        float critical = r.end_step();
        check(std::abs(critical - 5.0f) < 1e-3f,
              "RewardSystem: 5x milestone stack lands at +5 (D-062 critical)");

        // D-062: critical milestone honours crisis scaling (so a starving
        // agent in vital crisis only gets 0.3x of the first_hunt bonus,
        // matching how first_eat/first_drink already behaved).
        r.reset_episode();
        r.begin_step();
        for (int i = 0; i < 5; ++i) r.event_milestone(0.3f);
        float critical_crisis = r.end_step();
        check(std::abs(critical_crisis - 1.5f) < 1e-3f,
              "RewardSystem: 5x milestone stack respects crisis scale (D-062)");

        // D-062: fur collection on prey kill should fire event_collected_resource,
        // i.e. each fur drop is worth +0.5 reward (matches collect of wood/stone).
        // This gives PPO a per-kill signal even when the agent isn't hungry
        // enough for eat_drive to pay out, and even when first_hunt has
        // already been claimed this episode.
        r.reset_episode();
        r.begin_step();
        r.event_collected_resource();
        float fur_bump = r.end_step();
        check(std::abs(fur_bump - 0.5f) < 1e-3f,
              "RewardSystem: fur on kill rewards as collect (+0.5) (D-062)");
    }

    // --- SpatialMemory ---
    {
        dp::agent::SpatialMemory sm;
        sm.configure(0.0f, 0.0f);
        check(sm.visited_count() == 0,
              "SpatialMemory: clears to zero on configure");

        // Cell at (0,0) -> grid (0,0)
        sm.write(8.0f, 8.0f, false, false, false, false, false);
        check(sm.visited_count() == 1,
              "SpatialMemory: writing inside a cell increments visited");

        // Same cell again: visited stays 1.
        sm.write(15.9f, 15.9f, true, false, false, false, false);
        check(sm.visited_count() == 1,
              "SpatialMemory: writing same cell does not double-count");
        check(sm.feature_avg(dp::agent::SpatialMemory::CH_WATER) > 0.0f,
              "SpatialMemory: water flag persists after write");

        // Out-of-bounds is a silent no-op.
        sm.write(-100.0f, -100.0f, true, true, true, true, true);
        check(sm.visited_count() == 1,
              "SpatialMemory: out-of-bounds writes are ignored");

        // Read window centered on agent
        std::array<float, dp::agent::SpatialMemory::WINDOW_DIM> win{};
        sm.read_window(12.0f, 12.0f, win.data());
        // Centre cell should be the one we just wrote -> non-zero visited
        // Window layout is (gy, gx, feature); centre is (half, half).
        const int half = dp::agent::SpatialMemory::WINDOW / 2;
        const int feat = dp::agent::SpatialMemory::FEATURES;
        int centre = (half * dp::agent::SpatialMemory::WINDOW + half) * feat;
        check(win[centre + dp::agent::SpatialMemory::CH_VISITED] > 0.5f,
              "SpatialMemory: read_window centre reflects current cell");

        // Boundary read (top-left corner of world) should have zeros for
        // the OOB half of the window.
        sm.read_window(0.0f, 0.0f, win.data());
        check(win[0] == 0.0f,
              "SpatialMemory: out-of-bounds window cells are zero");

        // Clear empties the grid.
        sm.clear();
        check(sm.visited_count() == 0,
              "SpatialMemory: clear() resets all cells");

        // D-049 #3: vision-range writes mark cells the agent can SEE,
        // not just the cell under its feet. Stamp a 'water' flag at a
        // location 50 m NE of the origin and confirm it survives in the
        // grid even though the agent never walked there.
        sm.write(50.0f, 50.0f, /*water*/true, false, false, false, false);
        // Read a window centred on the origin: cells (50,50) sit at grid
        // index (3,3) given CELL_SIZE_M=16; from the agent at (0,0) those
        // cells lie inside the WINDOW=8 patch and should expose water>0.
        std::array<float, dp::agent::SpatialMemory::WINDOW_DIM> win2{};
        sm.read_window(0.0f, 0.0f, win2.data());
        bool any_water_in_window = false;
        for (int i = 0; i < dp::agent::SpatialMemory::WINDOW
                          * dp::agent::SpatialMemory::WINDOW; ++i) {
            int off = i * dp::agent::SpatialMemory::FEATURES
                    + dp::agent::SpatialMemory::CH_WATER;
            if (win2[off] > 0.5f) { any_water_in_window = true; break; }
        }
        check(any_water_in_window,
              "SpatialMemory: remote write seen by read_window (D-049 #3)");
    }

    // --- D-049 #2: stamina-aware speed multiplier ---
    {
        check(std::abs(dp::env::Environment::stamina_speed_multiplier(50.0f)
                       - 1.0f) < 1e-5f,
              "Environment: stamina>=50 yields full speed (D-049 #2)");
        check(std::abs(dp::env::Environment::stamina_speed_multiplier(0.0f)
                       - 0.5f) < 1e-5f,
              "Environment: stamina=0 yields 0.5 speed floor (D-049 #2)");
        check(std::abs(dp::env::Environment::stamina_speed_multiplier(25.0f)
                       - 0.75f) < 1e-5f,
              "Environment: stamina=25 yields 0.75 speed (D-049 #2)");
        check(std::abs(dp::env::Environment::stamina_speed_multiplier(80.0f)
                       - 1.0f) < 1e-5f,
              "Environment: stamina>50 stays clamped at 1.0 (D-049 #2)");
    }

    // --- D-122 early-warning: per-action advantage grouping ---
    // The hunt-trend indicator is just "mean advantage of the attack action".
    // Verify the pure helper groups by action and reports the right sign/count,
    // because the SIGN is what we read to predict the trend early.
    {
        using dp::policy::PPO;
        // disc:  attack attack cook  eat  attack cook
        std::vector<int64_t> disc = {  2,    2,   18,   0,    2,   18 };
        // adv:   hunting clearly GOOD (+), cooking clearly BAD (-)
        std::vector<float>   adv  = {+1.0f,+2.0f,-3.0f,+0.5f,+0.0f,-1.0f};

        auto [a_mean, a_cnt] = PPO::mean_adv_for_action(adv, disc, /*attack*/2);
        check(a_cnt == 3, "adv-watch: counts all 3 attack transitions");
        check(std::abs(a_mean - 1.0f) < 1e-6f,
              "adv-watch: attack mean = (1+2+0)/3 = +1.0 (positive => hunt rises)");

        auto [c_mean, c_cnt] = PPO::mean_adv_for_action(adv, disc, /*cook*/18);
        check(c_cnt == 2, "adv-watch: counts both cook transitions");
        check(c_mean < 0.0f && std::abs(c_mean + 2.0f) < 1e-6f,
              "adv-watch: cook mean = (-3-1)/2 = -2.0 (negative => cook falls)");

        auto [m_mean, m_cnt] = PPO::mean_adv_for_action(adv, disc, /*mine,absent*/19);
        check(m_cnt == 0 && m_mean == 0.0f,
              "adv-watch: absent action => {0,0} (no false signal)");

        // Size-mismatch safety (shorter of the two is used, no OOB).
        std::vector<int64_t> short_disc = { 2, 2 };
        auto [s_mean, s_cnt] = PPO::mean_adv_for_action(adv, short_disc, 2);
        check(s_cnt == 2 && std::abs(s_mean - 1.5f) < 1e-6f,
              "adv-watch: tolerates size mismatch (uses min length)");

        // D-123 FULL-behavior panel: PPO::update fills adv_by_action /
        // cnt_by_action by running mean_adv_for_action over EVERY categorical
        // id. Verify that loop groups ALL behaviors with no leakage, so a few
        // hundred episodes predict which of every action rises/falls.
        {
            using dp::policy::ActorCriticImpl;
            std::array<float, ActorCriticImpl::DISC_CATS> padv{};
            std::array<int,   ActorCriticImpl::DISC_CATS> pcnt{};
            int sum_cnt = 0;
            for (int a = 0; a < ActorCriticImpl::DISC_CATS; ++a) {
                auto [m, c] = PPO::mean_adv_for_action(adv, disc, a);
                padv[a] = m; pcnt[a] = c; sum_cnt += c;
            }
            check(pcnt[2] == 3 && pcnt[18] == 2 && pcnt[0] == 1,
                  "panel: attack=3, cook=2, eat=1 grouped correctly");
            check(std::abs(padv[2] - 1.0f) < 1e-6f && padv[18] < 0.0f,
                  "panel: per-action signs match watched hunt(+)/cook(-)");
            check(pcnt[19] == 0 && padv[19] == 0.0f && pcnt[16] == 0,
                  "panel: untaken actions stay {0,0} (no false ignition)");
            check(sum_cnt == static_cast<int>(disc.size()),
                  "panel: counts partition all transitions (no double-count)");
            check(std::abs(100.0 * pcnt[2] / static_cast<double>(disc.size())
                           - 50.0) < 1e-6,
                  "panel: frequency = cnt/total (attack 3/6 = 50%)");
        }
    }

    // -----------------------------------------------------------------------
    // D-123 Successor-Features (ψ). Six checks incl. two negative examples,
    // per docs/Successor_Features_设计.md §4. These gate every SF training run.
    // -----------------------------------------------------------------------
    {
        using dp::agent::RewardSystem;
        using dp::policy::ActorCriticImpl;
        constexpr int F = RewardSystem::F;

        // SF1 — φ reconstruction: dot(last_features, weight_vector) reproduces
        // the UNCLIPPED step total exactly (blended buckets carry w=1).
        {
            RewardSystem rs;                     // default S5 weights
            rs.begin_step();
            rs.event_alive(0.5f);
            rs.event_eat(0.4f);
            rs.event_drink(0.2f);
            rs.event_wolf_bite(3.0f);
            dp::agent::VitalState v;             // any state; reconstruction is exact
            v.hunger = 40.0f; v.thirst = 30.0f; v.stamina = 60.0f;
            v.temperature = 35.0f; v.protein = 50.0f; v.vitamin = 70.0f;
            rs.event_vital_pressure(v, 0.5f);
            rs.event_vital_danger(v, 0.5f);
            rs.event_action_cost(0.001f);
            rs.event_collected_resource();
            rs.event_deposited_material();
            rs.event_built_shelter();
            rs.event_crafted_tool();
            rs.event_milestone(0.7f);
            rs.event_state_bonus(2.0f);
            (void)rs.end_step();
            const auto  phi = rs.last_features();
            const auto  w   = rs.weight_vector();
            double dot = 0.0;
            for (int f = 0; f < F; ++f) dot += static_cast<double>(phi[f]) * w[f];
            const double total = rs.last_step().total();
            check(std::abs(dot - total) < 1e-4,
                  "SF φ-recon: dot(last_features, weight_vector) == unclipped total");
            // negative-control sanity: kills channel is genuinely 0 this step.
            check(phi[3] == 0.0f, "SF φ-recon: untriggered kills channel φ==0");
        }

        // SF2 — ψ Bellman fixed point (no terminal): for a constant feature φ
        // the TD target ψ <- φ + γψ has the fixed point ψ* = φ/(1-γ). Mirror the
        // exact host recursion used in PPO::update and confirm it is stationary.
        {
            const float gamma = 0.995f;
            std::array<float, F> phi_c{};
            for (int f = 0; f < F; ++f) phi_c[f] = 0.1f * (f + 1);
            std::array<float, F> psi_star{};
            for (int f = 0; f < F; ++f) psi_star[f] = phi_c[f] / (1.0f - gamma);
            // one Bellman backup from the fixed point must return the fixed point
            bool stationary = true;
            for (int f = 0; f < F; ++f) {
                const float backup = phi_c[f] + gamma * /*nonterm*/1.0f * psi_star[f];
                if (std::abs(backup - psi_star[f]) > 1e-2f) stationary = false;
            }
            check(stationary, "SF ψ-fixedpoint: φ+γψ* == ψ* (ψ*=φ/(1-γ))");
        }

        // SF3 — done truncation: at a terminal step the (1-done) factor zeroes
        // the bootstrap, so the target collapses to φ alone (no post-done leak).
        {
            const float gamma = 0.995f;
            const float phi_t = 0.7f, psi_next = 5.0f;
            const float tgt_live = phi_t + gamma * 1.0f * psi_next;
            const float tgt_done = phi_t + gamma * 0.0f * psi_next;
            check(std::abs(tgt_done - phi_t) < 1e-6f && tgt_live > tgt_done,
                  "SF done-trunc: terminal target == φ (bootstrap dropped)");
        }

        // SF4 — NEGATIVE example A (re-weight rank). Offline value V'(s)=ψ(s)·w'.
        // Raising the kills weight must raise V' for a ψ that accumulates kills,
        // and must NOT change V' for a ψ with zero kills mass (no phantom value).
        {
            std::array<float, F> w_lo = RewardSystem{}.weight_vector();
            std::array<float, F> w_hi = w_lo; w_hi[3] = w_lo[3] + 3.0f;  // kills↑
            std::array<float, F> psi_kills{}; psi_kills[3] = 2.0f;       // future kills
            std::array<float, F> psi_nokill{};                          // all zero
            auto dot = [&](const std::array<float,F>& a, const std::array<float,F>& b){
                double s = 0.0; for (int f=0; f<F; ++f) s += (double)a[f]*b[f]; return s; };
            const double v_lo = dot(psi_kills,  w_lo);
            const double v_hi = dot(psi_kills,  w_hi);
            const double n_lo = dot(psi_nokill, w_lo);
            const double n_hi = dot(psi_nokill, w_hi);
            check(v_hi > v_lo && std::abs(n_hi - n_lo) < 1e-9,
                  "SF re-weight: kills↑ raises V' iff ψ has kills mass (else inert)");
        }

        // SF5 — NEGATIVE example B (zero-feature isolation). A channel whose φ is
        // identically 0 over a trajectory keeps ψ-target 0 and never pollutes the
        // other channels' targets through the vector Bellman backup.
        {
            const float gamma = 0.995f;
            const int K = 3;                         // the silent channel
            std::array<float, F> phi_step{};
            for (int f = 0; f < F; ++f) phi_step[f] = (f == K) ? 0.0f : 0.2f;
            std::array<float, F> psi{};              // start at 0
            for (int it = 0; it < 2000; ++it)        // iterate the backup to convergence
                for (int f = 0; f < F; ++f)
                    psi[f] = phi_step[f] + gamma * psi[f];
            bool silent_zero = std::abs(psi[K]) < 1e-6f;
            bool others_nonzero = true;
            for (int f = 0; f < F; ++f)
                if (f != K && psi[f] < 1.0f) others_nonzero = false;  // ~0.2/(1-γ)=40
            check(silent_zero && others_nonzero,
                  "SF zero-feature: silent channel ψ==0, others unaffected");
        }

        // SF6 — psi() head shape + DETACHED-trunk gradient (the zero-policy-
        // perturbation red line). Loss on ψ(h.detach()) must give the successor
        // head a gradient while leaving the encoder/policy trunk untouched.
        {
            torch::manual_seed(7);
            dp::policy::ActorCritic net;
            net->to(torch::kCPU);
            const int B = 4;
            auto obs = torch::randn({B, ActorCriticImpl::OBS_DIM});
            auto h0  = torch::zeros({B, ActorCriticImpl::HIDDEN});
            auto fo  = net->forward_step(obs, h0);
            auto h   = std::get<4>(fo);
            auto psi = net->psi(h.detach());                  // [B, F]
            check(psi.size(0) == B && psi.size(1) == F,
                  "SF psi(): output shape [B, SF_DIM]");
            net->zero_grad();
            auto loss = (psi - torch::zeros_like(psi)).pow(2).mean();
            loss.backward();
            const bool enc_clean = !net->encoder->weight.grad().defined();
            const bool suc_grad  =  net->successor->weight.grad().defined();
            check(enc_clean && suc_grad,
                  "SF detach: ψ loss updates successor only (trunk grad == none)");
        }
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

    // CLI for the viewer. Defaults to "manual" (keyboard) which is what
    // we've always had. With --policy ppo --load file.pt the agent will
    // be driven by a trained network instead of the human; movement keys
    // (E/Q/F/...) are still listened to as overrides if you want to nudge
    // the agent during a demo.
    std::string viewer_policy = "manual";
    std::string viewer_load_path;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                spdlog::error("missing value for {}", a);
                std::exit(2);
            }
            return std::string(argv[++i]);
        };
        if      (a == "--policy") viewer_policy    = next();
        else if (a == "--load")   viewer_load_path = next();
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

        dp::agent::ProgressTracker progress;
        dp::world::BuildingSystem  buildings;
        // What building type the agent will place next time it hits P.
        dp::world::BuildingType    selected_building = dp::world::BuildingType::Shed;

        dp::agent::Wardrobe wardrobe;

        dp::world::FarmingSystem farming;
        // Last plant kind successfully harvested - used when planting a
        // seed so the resulting plot matches what the agent has eaten.
        dp::world::PlantKind last_seed_kind = dp::world::PlantKind::Berry;

        dp::agent::TameSystem tame;

        // Main-line milestone flags (one-shot rewards). Persist across
        // episodes so the agent only earns each milestone once per
        // training session.
        bool first_house_built_fired    = false;
        bool first_crop_harvested_fired = false;
        bool first_follower_fired       = false;

        dp::agent::VitalSystem vitals;
        dp::agent::VitalState  vital_state{};

        // RewardSystem: per-tick scalar that the PPO policy will optimize
        // against once we plug it in (S4). For S3 we run it in shadow mode
        // so we get a feel for how the reward signal behaves under wander.
        dp::agent::RewardSystem reward;

        // D-043 P1: drive function + eat/drink helpers (Keramati 2014).
        // Mirrors Environment.cpp so the viewer and headless trainer compute
        // identical rewards. drive(v=100)=0, drive(v=0)=1, quadratic.
        auto drive_v = [](float vital) {
            const float gap = (100.0f - std::clamp(vital, 0.0f, 100.0f)) / 100.0f;
            return gap * gap;
        };
        auto eat_drive = [&](float kcal) -> float {
            if (kcal <= 0.0f) return 0.0f;
            const float before = drive_v(vital_state.hunger);
            const float ate    = vitals.eat(vital_state, kcal);
            const float after  = drive_v(vital_state.hunger);
            reward.event_eat(before - after);
            return ate;
        };
        auto drink_drive = [&](float ml) -> float {
            if (ml <= 0.0f) return 0.0f;
            const float before = drive_v(vital_state.thirst);
            const float drunk  = vitals.drink(vital_state, ml);
            const float after  = drive_v(vital_state.thirst);
            reward.event_drink(before - after);
            return drunk;
        };

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

        // Optional PPO policy (loaded from a checkpoint). When present,
        // the main loop uses sample_action() in place of wander, and
        // SKIPS the keyboard overlay so trained behaviour is the only
        // input. Pass --policy ppo --load file.pt to enable.
        std::unique_ptr<dp::policy::PPO> ppo_policy;
        // SpatialMemory used both for HUD-visualisable cognitive map and
        // (when --policy ppo) as part of the observation vector.
        dp::agent::SpatialMemory viewer_spatial;
        viewer_spatial.configure(0.0f, 0.0f);
        if (viewer_policy == "ppo") {
            dp::policy::PPOConfig pcfg;
            ppo_policy = std::make_unique<dp::policy::PPO>(pcfg);
            if (!viewer_load_path.empty()) {
                ppo_policy->load(viewer_load_path);
                spdlog::info("Viewer running with PPO policy from '{}'",
                             viewer_load_path);
            } else {
                spdlog::warn("--policy ppo set but no --load given; using "
                             "freshly-initialised network (random behaviour)");
            }
        }

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

            // Compose this tick's AgentAction. Two paths:
            //   * PPO-driven (when ppo_policy is set): build the 145-dim
            //     observation, run sample_action(), and use whatever the
            //     network returned. Keyboard is ignored so trained
            //     behaviour isn't contaminated.
            //   * Manual (default): wander gives a base move direction;
            //     keyboard triggers stamp on top.
            dp::agent::AgentAction action;
            if (ppo_policy) {
                dp::agent::ObservationBuilder::Vector obs;
                dp::agent::ObservationBuilder::compose(
                    world, agent, vital_state, day,
                    plants, animals, resources,
                    inventory, shelter, episodes, progress,
                    buildings, wardrobe, farming, tame,
                    selected_building, viewer_spatial,
                    // Stage W1: the raylib viewer doesn't yet simulate the sky,
                    // so feed clear/dry (0,0). Matches weather-off training and
                    // keeps obs at 573 dims. Live weather render is a later pass.
                    0.0f, 0.0f, obs);
                std::vector<float> cont;
                int  disc_idx = dp::policy::ActorCriticImpl::NOOP_IDX;
                float lp = 0.0f, vv = 0.0f;
                action = ppo_policy->sample_action(obs, cont, disc_idx, lp, vv);
            } else {
                action = agent.sample_wander_action(dt);
                action.eat                 = IsKeyPressed(KEY_E);
                action.drink               = IsKeyPressed(KEY_Q);
                action.attack              = IsKeyPressed(KEY_F);
                action.collect             = IsKeyPressed(KEY_R);  // shared key
                action.deposit_to_site     = IsKeyPressed(KEY_R);  // R prioritises deposit
                action.place_shelter       = IsKeyPressed(KEY_B);
                action.craft_spear         = IsKeyPressed(KEY_C);
                action.craft_grass_dress   = IsKeyPressed(KEY_X);
                action.craft_fur_cloak     = IsKeyPressed(KEY_Z);
                action.wear_clothes        = IsKeyPressed(KEY_V);
                action.place_blueprint     = IsKeyPressed(KEY_P);
                action.cycle_building_type = IsKeyPressed(KEY_TAB);
                action.plant_seed          = IsKeyPressed(KEY_N);
                action.water_plot          = IsKeyPressed(KEY_J);
                action.feed_tame           = IsKeyPressed(KEY_K);
            }
            agent.apply_action(action, world, physics, dt);
            const auto& as = agent.state();

            // Write current sense into the cognitive map (mirrors what
            // Environment does). 60m radius scan; we only mark booleans.
            {
                const float R  = dp::agent::ObservationBuilder::VISION_RANGE;
                const float R2 = R * R;
                bool saw_water = false;
                {
                    auto b = world.biome_at(as.pos_x, as.pos_y);
                    saw_water = (b == dp::world::Biome::River
                                 || b == dp::world::Biome::Swamp);
                }
                bool saw_food = false;
                for (const auto& pl : plants.plants()) {
                    if (!pl.ripe) continue;
                    float dx = pl.x - as.pos_x, dy = pl.y - as.pos_y;
                    if (dx*dx + dy*dy <= R2) { saw_food = true; break; }
                }
                bool saw_resource = false;
                for (const auto& rs : resources.resources()) {
                    if (!rs.available) continue;
                    float dx = rs.x - as.pos_x, dy = rs.y - as.pos_y;
                    if (dx*dx + dy*dy <= R2) { saw_resource = true; break; }
                }
                bool saw_danger = false;
                for (const auto& an : animals.animals()) {
                    if (an.state == dp::world::AnimalState::Dead) continue;
                    if (an.kind != dp::world::AnimalKind::Wolf) continue;
                    float dx = an.pos_x - as.pos_x, dy = an.pos_y - as.pos_y;
                    if (dx*dx + dy*dy <= R2) { saw_danger = true; break; }
                }
                bool saw_building = false;
                for (const auto& bd : buildings.buildings()) {
                    float dx = bd.x - as.pos_x, dy = bd.y - as.pos_y;
                    if (dx*dx + dy*dy <= R2) { saw_building = true; break; }
                }
                for (const auto& site : buildings.sites()) {
                    if (site.completed) continue;
                    float dx = site.x - as.pos_x, dy = site.y - as.pos_y;
                    if (dx*dx + dy*dy <= R2) { saw_building = true; break; }
                }
                viewer_spatial.write(as.pos_x, as.pos_y,
                                     saw_water, saw_food, saw_resource,
                                     saw_danger, saw_building);
            }

            reward.begin_step();
            reward.event_alive(dt);

            // Progression: every metre travelled while alive counts.
            float step_speed = std::sqrt(as.vel_x*as.vel_x + as.vel_y*as.vel_y);
            progress.on_explore_step(step_speed * dt);

            // Animals follow the agent's position; wolves at night get
            // boosted perception+speed via night_factor. Shelter dampens
            // wolf detection of the agent (S3.8c). Real buildings give
            // even better coverage (S3.10a) - both stacking is fine.
            float night_factor = std::max(0.0f, -day.sun_height_normalized());
            float perc_scale = 1.0f, spd_scale = 1.0f;
            bool  sheltered = shelter.covers(as.pos_x, as.pos_y)
                           || buildings.covers(as.pos_x, as.pos_y);
            if (sheltered) {
                perc_scale = 0.3f;
                spd_scale  = 0.7f;
            }
            animals.tick(world, as.pos_x, as.pos_y, as.pos_z, dt, night_factor,
                         perc_scale, spd_scale);
            // Tamed followers stay near the agent regardless of their AI
            // state. Late stage only - keeps the curriculum tidy.
            if (progress.can_tame()) {
                tame.apply_follow(animals, as.pos_x, as.pos_y, dt);
            }

            // Plants regrow timers
            plants.tick(dt);
            resources.tick(dt);
            farming.tick(dt);

            // Vitals (with movement speed for stamina)
            float speed = std::sqrt(as.vel_x*as.vel_x + as.vel_y*as.vel_y);
            vitals.tick(vital_state, world, as.pos_x, as.pos_y, speed, day, dt,
                        wardrobe.cold_resist());
            // Inside shelter or building: body temp drifts toward
            // neutral fast. Building gives the same effect (stronger
            // would also work but keep it simple for now).
            if (shelter.covers(as.pos_x, as.pos_y)
                || buildings.covers(as.pos_x, as.pos_y)) {
                vital_state.temperature += (50.0f - vital_state.temperature)
                                          * 0.3f * dt;
            }
            reward.event_vital_pressure(vital_state, dt);

            // E -> action.eat: harvest any nearby ripe plant. Mushrooms
            // have a 30% chance to poison instead of feed (blueprint
            // line 16). Late stage: same trigger also picks up ripe farm
            // plots.
            if (action.eat) {
                auto got = plants.try_harvest(as.pos_x, as.pos_y,
                                              as.pos_z + as.height * 0.5f, 2.5f);
                if (progress.can_farm()) {
                    auto farm_got = farming.harvest_at(as.pos_x, as.pos_y, 2.0f);
                    for (auto k : farm_got) got.push_back(k);
                    if (!farm_got.empty() && !first_crop_harvested_fired) {
                        reward.event_milestone();
                        first_crop_harvested_fired = true;
                    }
                }
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
                        // Drop hunger and stamina; D-043 P1: inverse
                        // drive_red of -0.2 ≈ -0.6 reward at coef=3.0.
                        vital_state.hunger  = std::max(0.0f, vital_state.hunger  - 12.0f);
                        vital_state.stamina = std::max(0.0f, vital_state.stamina - 25.0f);
                        reward.event_eat(-0.2f);
                    } else {
                        eat_drive(kcal);
                        drink_drive(ml);
                    }
                    // 30% chance to also drop a Seed for Fruit/Berry
                    // (used to plant farms in late stage).
                    if (k == dp::world::PlantKind::Fruit
                        || k == dp::world::PlantKind::Berry) {
                        unsigned r = static_cast<unsigned>(
                            (uint64_t)(as.pos_x*991.0f) * 7919ULL
                          ^ (uint64_t)(GetTime() * 1e6f));
                        if ((r % 100u) < 30u) {
                            inventory.add(dp::agent::ItemKind::Seed);
                            last_seed_kind = k;
                        }
                    }
                    ++total_harvested;
                }
            }

            // Q -> action.drink: drink water if standing in/near a
            // river or swamp; same tick can also catch a fish in arm's reach.
            if (action.drink) {
                auto b = world.biome_at(as.pos_x, as.pos_y);
                if (b == dp::world::Biome::River || b == dp::world::Biome::Swamp) {
                    drink_drive(35.0f);
                    // Same key: try to catch a fish in arm's reach.
                    auto fish = animals.agent_attack(as.pos_x, as.pos_y,
                                                     agent.yaw(), 2.0f, 100.0f);
                    if (fish.killed
                        && animals.animals()[fish.index].kind
                            == dp::world::AnimalKind::Fish) {
                        eat_drive(20.0f);
                    }
                }
            }

            // R -> action.deposit_to_site (priority) OR action.collect
            // (fallback). The two flags share the keyboard so for human
            // play they fire together; PPO can fire them independently
            // (which lets a learnt policy choose to ONLY pick up or ONLY
            // deposit per tick).
            if (action.deposit_to_site || action.collect) {
                bool deposited = false;
                if (action.deposit_to_site && progress.can_build()) {
                    for (int slot = 0; slot < inventory.size(); ++slot) {
                        auto k = inventory.at(slot);
                        if (k != dp::agent::ItemKind::Wood
                         && k != dp::agent::ItemKind::Stone
                         && k != dp::agent::ItemKind::Grass) continue;
                        int site_idx = buildings.try_deposit(
                            as.pos_x, as.pos_y, as.pos_z,
                            2.5f, k);
                        if (site_idx >= 0) {
                            inventory.remove_one(k);
                            reward.event_collected_resource();  // small reward per deposit
                            deposited = true;
                            break;  // one deposit per tick
                        }
                    }
                }
                if (!deposited && action.collect) {
                    auto picked = resources.try_collect(as.pos_x, as.pos_y,
                                                        as.pos_z + as.height * 0.5f,
                                                        2.0f);
                    for (auto rk : picked) {
                        if (inventory.add(dp::agent::item_from_resource(rk))) {
                            reward.event_collected_resource();
                            progress.on_collect();
                        }
                    }
                }
            }

            // P -> action.place_blueprint: place a new construction site
            // at the agent's feet, using the currently selected
            // BuildingType. Gated by Mid stage inside place_site.
            if (action.place_blueprint) {
                int site = buildings.place_site(selected_building,
                                                as.pos_x, as.pos_y, as.pos_z,
                                                progress);
                (void)site;  // success/failure read from buildings.sites()
            }

            // Tab -> action.cycle_building_type: rotate next-to-place type.
            if (action.cycle_building_type) {
                int t = static_cast<int>(selected_building);
                t = (t + 1) % 4;
                selected_building = static_cast<dp::world::BuildingType>(t);
            }

            // Each tick: promote any site that just got its last material.
            int finished = buildings.tick_finalize();
            for (int i = 0; i < finished; ++i) {
                reward.event_built_shelter();
                progress.on_house_built();
                // S3.10 milestone: first house ever - one-shot +20.
                if (!first_house_built_fired) {
                    reward.event_milestone();
                    first_house_built_fired = true;
                }
            }

            // Stage unlock bonus (Early->Mid, Mid->Late). Each unlock
            // fires the milestone bonus twice (= +40) to mark the
            // moment as a hard inflection point for the policy.
            if (progress.poll_unlock()) {
                reward.event_milestone();
                reward.event_milestone();
            }

            // C -> action.craft_spear: 1 Wood + 1 Stone -> 1 Spear.
            if (action.craft_spear) {
                if (inventory.craft_spear()) {
                    reward.event_crafted_tool();
                }
            }

            // B -> action.place_shelter: 1 Wood + 1 Grass -> lean-to
            // anchor at feet. Replaces any existing anchor.
            if (action.place_shelter) {
                if (shelter.try_place(inventory,
                                      as.pos_x, as.pos_y, as.pos_z)) {
                    reward.event_built_shelter();
                }
            }

            // F -> action.attack: melee attack the closest animal in
            // front (90 deg arc). Spear in inventory boosts reach and
            // damage. Killed mammals also drop a Fur if room.
            if (action.attack) {
                bool has_spear = inventory.num_of(dp::agent::ItemKind::Spear) > 0;
                float reach  = has_spear ? 2.5f : 1.5f;
                float damage = has_spear ? 50.0f : 20.0f;
                auto hit = animals.agent_attack(as.pos_x, as.pos_y,
                                                agent.yaw(), reach, damage);
                if (hit.killed) {
                    auto kind = animals.animals()[hit.index].kind;
                    if (kind == dp::world::AnimalKind::Wolf) {
                        reward.event_wolf_killed();
                        eat_drive(40.0f);
                        inventory.add(dp::agent::ItemKind::Fur);
                    } else if (kind == dp::world::AnimalKind::Rabbit) {
                        eat_drive(15.0f);
                        inventory.add(dp::agent::ItemKind::Fur);
                    } else if (kind == dp::world::AnimalKind::Deer) {
                        eat_drive(40.0f);
                        inventory.add(dp::agent::ItemKind::Fur);
                    } else if (kind == dp::world::AnimalKind::Fish) {
                        eat_drive(20.0f);
                    }
                }
            }

            // X -> action.craft_grass_dress: 3 Grass -> 1 GrassDress.
            if (action.craft_grass_dress) {
                if (inventory.craft_grass_dress()) {
                    reward.event_crafted_tool();
                }
            }
            // Z -> action.craft_fur_cloak: 2 Fur -> 1 FurCloak.
            if (action.craft_fur_cloak) {
                if (inventory.craft_fur_cloak()) {
                    reward.event_crafted_tool();
                }
            }
            // V -> action.wear_clothes: put on the best available
            // clothing (cloak > dress).
            if (action.wear_clothes) {
                if (inventory.num_of(dp::agent::ItemKind::FurCloak) > 0) {
                    wardrobe.wear(inventory, dp::agent::ItemKind::FurCloak);
                } else if (inventory.num_of(dp::agent::ItemKind::GrassDress) > 0) {
                    wardrobe.wear(inventory, dp::agent::ItemKind::GrassDress);
                }
            }

            // N -> action.plant_seed (Late only): plant the seed in
            // inventory at the agent's feet, creating a FarmPlot of the
            // same kind as the last fruit/berry harvested.
            if (action.plant_seed && progress.can_farm()) {
                if (inventory.num_of(dp::agent::ItemKind::Seed) > 0) {
                    inventory.remove_one(dp::agent::ItemKind::Seed);
                    farming.plant_seed(as.pos_x, as.pos_y, as.pos_z,
                                       last_seed_kind);
                    reward.event_collected_resource();  // small reward for planting
                }
            }
            // J -> action.water_plot (Late only): water nearest plot
            // within 2m.
            if (action.water_plot && progress.can_farm()) {
                if (farming.water_at(as.pos_x, as.pos_y, 2.0f)) {
                    // Small stamina cost so spamming isn't free.
                    vital_state.stamina = std::max(0.0f,
                        vital_state.stamina - 2.0f);
                }
            }

            // K -> action.feed_tame (Late only): feed grass to nearest
            // prey or fur to a wolf. Builds trust; once trust >= 0.5 the
            // animal follows.
            if (action.feed_tame && progress.can_tame()) {
                auto fr = tame.try_feed(animals, inventory,
                                        as.pos_x, as.pos_y, 3.0f);
                if (fr.animal_idx >= 0) {
                    inventory.remove_one(fr.consumed);
                    reward.event_collected_resource();
                    if (fr.just_became_follower) {
                        reward.event_crafted_tool();
                        // S3.10 milestone: first follower ever -> +20.
                        if (!first_follower_fired) {
                            reward.event_milestone();
                            first_follower_fired = true;
                        }
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

            // Death edge: count the -10 in this step's reward and let
            // EpisodeManager handle the actual reset *after* end_step().
            if (was_alive && !vital_state.alive) {
                reward.event_death();
            }
            was_alive = vital_state.alive;

            // D-043 P2: per-step state-based bonus (Φ = house + clothes +
            // followers + inventory diversity). Mirrors Environment.cpp.
            {
                float phi = 0.0f;
                if (!buildings.buildings().empty()) phi += 0.5f;
                if (wardrobe.worn() != dp::agent::ItemKind::Count) phi += 0.3f;
                phi += std::min(0.5f, 0.1f * static_cast<float>(tame.follower_count()));
                int unique = 0;
                for (int k = 0; k < static_cast<int>(dp::agent::ItemKind::Count); ++k) {
                    if (inventory.num_of(static_cast<dp::agent::ItemKind>(k)) > 0) {
                        ++unique;
                    }
                }
                phi += std::min(0.5f, 0.05f * static_cast<float>(unique));
                reward.event_state_bonus(phi);
            }

            reward.end_step();
            episodes.tick(dt);

            // End-of-episode: log totals and reset state. Terrain/plants
            // stay untouched; agent + vitals + animals are reseeded.
            if (episodes.is_done(vital_state)) {
                spdlog::info(
                    "Episode {} done: steps={} elapsed={:.1f}s "
                    "reward={:+.1f} (alive={:+.1f} food={:+.1f} water={:+.1f} "
                    "kills={:+.1f} bites={:+.1f} death={:+.1f} vital={:+.1f} "
                    "milestone={:+.1f})",
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
                    reward.episode().r_vital,
                    reward.episode().r_milestone);
                episodes.reset(world, agent, vitals, vital_state,
                               reward, animals, inventory, shelter,
                               buildings);
                wardrobe.clear();
                farming.clear();
                tame.clear();
                viewer_spatial.clear();
                if (ppo_policy) ppo_policy->reset_hidden();
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
                          resources, shelter, buildings, farming,
                          wardrobe.worn(),
                          day.terrain_tint());

            // HUD background
            const auto& s = agent.state();
            DrawRectangle(8, 8, 480, 420, Color{ 0, 0, 0, 160 });
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
            // Progression line
            const char* stage_name = dp::agent::progress_stage_name(progress.stage());
            DrawText(TextFormat("Stage: %s   collected=%d/%d  explored=%.0f/%.0fm  houses=%d",
                                stage_name,
                                progress.resources_collected(),
                                progress.mid_collect_threshold,
                                progress.total_explored(),
                                progress.mid_explore_threshold,
                                progress.houses_built()),
                     12, 254, 14, Color{ 200, 200, 255, 255 });
            // Building selector + active sites count
            const auto& brec = dp::world::building_recipe(selected_building);
            DrawText(TextFormat("Selected: %s [W%d S%d G%d]  Sites=%zu  Built=%zu",
                                brec.name,
                                brec.wood_required,
                                brec.stone_required,
                                brec.grass_required,
                                buildings.sites().size(),
                                buildings.buildings().size()),
                     12, 270, 14, Color{ 220, 255, 200, 255 });
            // Wardrobe
            const char* worn_name = "none";
            if (wardrobe.wearing(dp::agent::ItemKind::GrassDress)) worn_name = "grass dress (+10% cold)";
            else if (wardrobe.wearing(dp::agent::ItemKind::FurCloak)) worn_name = "fur cloak (+30% cold)";
            DrawText(TextFormat("Worn: %s  Fur=%d  Seeds=%d",
                                worn_name,
                                inventory.num_of(dp::agent::ItemKind::Fur),
                                inventory.num_of(dp::agent::ItemKind::Seed)),
                     12, 286, 14, Color{ 255, 220, 200, 255 });
            // Farming + Taming line (Late stage)
            DrawText(TextFormat("Farm: %d plots (%d ripe)   Tame: %d followers",
                                farming.total_count(),
                                farming.ripe_count(),
                                tame.follower_count()),
                     12, 302, 14, Color{ 200, 255, 200, 255 });
            DrawText(TextFormat("Camera: %s   [G] toggle follow",
                                follow_agent ? "follow" : "fly"),
                     12, 322, 16, RAYWHITE);
            DrawText("E=harvest   Q=drink/fish   R=collect/deposit   F=attack",
                     12, 342, 14, Color{ 220, 220, 100, 255});
            DrawText("C=spear   X=dress   Z=cloak   V=wear   B=quick shelter",
                     12, 358, 14, Color{ 220, 220, 100, 255});
            DrawText("Tab=cycle building   P=place site   N=plant seed   J=water",
                     12, 374, 14, Color{ 220, 220, 100, 255});
            DrawText("K=feed/tame   WASD fly  Ctrl=fast  T/Y=time  Esc=quit",
                     12, 390, 14, Color{ 180, 180, 180, 255 });

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
