// deep_protagonist · headless trainer entry point.
//
// This is the binary the PPO loop will eventually drive. For S4-pre it
// just runs the Environment with a random/wander policy so we can:
//   * verify that Environment matches the viewer logic,
//   * benchmark headless tick rate,
//   * produce a baseline log to compare PPO against.
//
// CLI:
//   deep_protagonist_train --steps 10000          (default 10000)
//                          --episodes 100         (cap on episodes)
//                          --seed 12345           (world seed; default 12345)
//                          --policy wander|noop   (default wander)
//                          --metrics path.jsonl   (optional, append-mode)

#include "env/Environment.hpp"
#include "agent/AgentAction.hpp"
#include "policy/PPO.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <toml++/toml.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace {

dp::world::World::Config load_world_config(const std::string& path) {
    dp::world::World::Config cfg;
    if (!std::filesystem::exists(path)) {
        spdlog::warn("Config file '{}' missing; using defaults.", path);
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

// Wander policy: random 2D heading every ~2s (mirrors CapsuleAgent's
// wander), 5% chance per step to fire each interaction trigger. Gives a
// noisy baseline policy that DOES touch every action channel - useful
// for shaking out env bugs.
struct WanderPolicy {
    std::mt19937_64 rng;
    float dirx = 1.0f, diry = 0.0f;
    float change_in = 0.0f;

    explicit WanderPolicy(uint64_t seed) : rng(seed ? seed : 1) {}

    float frand01() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    }

    bool roll(float p) { return frand01() < p; }

    dp::agent::AgentAction sample(float dt) {
        change_in -= dt;
        if (change_in <= 0.0f) {
            float ang = frand01() * 6.2831853f;
            dirx = std::cos(ang);
            diry = std::sin(ang);
            change_in = 2.0f;
        }
        dp::agent::AgentAction a;
        a.move_x = dirx;
        a.move_y = diry;
        // Each interaction has a small chance per step to fire.
        a.eat                 = roll(0.02f);
        a.drink               = roll(0.02f);
        a.attack              = roll(0.01f);
        a.collect             = roll(0.05f);
        a.deposit_to_site     = roll(0.02f);
        a.place_shelter       = roll(0.001f);
        a.craft_spear         = roll(0.001f);
        a.craft_grass_dress   = roll(0.001f);
        a.craft_fur_cloak     = roll(0.001f);
        a.wear_clothes        = roll(0.005f);
        a.place_blueprint     = roll(0.0005f);
        a.cycle_building_type = roll(0.005f);
        a.plant_seed          = roll(0.005f);
        a.water_plot          = roll(0.01f);
        a.feed_tame           = roll(0.005f);
        return a;
    }
};

}  // namespace

int main(int argc, char** argv) {
    auto console = spdlog::stdout_color_mt("deep_protagonist_train");
    spdlog::set_default_logger(console);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    // CLI parsing (minimal hand-rolled, no extra deps)
    int      max_steps    = 10000;
    int      max_episodes = 0;            // 0 = unlimited
    uint64_t seed         = 12345;
    std::string policy    = "wander";
    std::string metrics_path;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                spdlog::error("missing value for {}", a);
                std::exit(2);
            }
            return std::string(argv[++i]);
        };
        if      (a == "--steps")    max_steps    = std::stoi(next());
        else if (a == "--episodes") max_episodes = std::stoi(next());
        else if (a == "--seed")     seed         = std::stoull(next());
        else if (a == "--policy")   policy       = next();
        else if (a == "--metrics")  metrics_path = next();
        else if (a == "-h" || a == "--help") {
            std::printf("Usage: %s [--steps N] [--episodes N] [--seed S] "
                        "[--policy wander|noop] [--metrics path.jsonl]\n",
                        argv[0]);
            return 0;
        } else {
            spdlog::warn("Unknown arg '{}', ignoring.", a);
        }
    }

    spdlog::info("================================================");
    spdlog::info(" deep_protagonist · headless trainer (S4-pre)");
    spdlog::info("================================================");
    spdlog::info("policy={}, steps={}, episodes_cap={}, seed={}",
                 policy, max_steps, max_episodes, seed);

    // Build EnvConfig from default.toml world settings, override seed.
    dp::env::EnvConfig ecfg;
    ecfg.world      = load_world_config("configs/default.toml");
    ecfg.world.seed = seed;
    ecfg.fixed_dt   = 1.0f / 30.0f;

    spdlog::info("Building world: {}x{} cells @ {}m, max_height={}m",
                 ecfg.world.heightmap_cells_x, ecfg.world.heightmap_cells_y,
                 ecfg.world.cell_scale, ecfg.world.terrain_max_height);

    dp::env::Environment env(ecfg);
    spdlog::info("Environment built. obs_dim={}", env.observation_size());

    // Optional metrics file (one JSON object per line).
    std::FILE* mf = nullptr;
    if (!metrics_path.empty()) {
        mf = std::fopen(metrics_path.c_str(), "ab");
        if (!mf) {
            spdlog::error("Could not open metrics file '{}'", metrics_path);
            return 1;
        }
        spdlog::info("Metrics will append to: {}", metrics_path);
    }

    WanderPolicy wander(seed ^ 0xC0DEULL);
    dp::agent::ObservationBuilder::Vector obs{};
    env.reset(obs);

    // PPO setup (only built if requested).
    std::unique_ptr<dp::policy::PPO> ppo;
    std::vector<dp::policy::Transition> rollout;
    dp::policy::PPOConfig pcfg;
    if (policy == "ppo") {
        pcfg.seed = static_cast<int>(seed & 0x7FFFFFFFULL);
        ppo = std::make_unique<dp::policy::PPO>(pcfg);
        rollout.reserve(pcfg.rollout_steps);
    }

    auto log_episode = [&](int episode_id,
                           const dp::agent::RewardSystem::EpisodeStats& s) {
        spdlog::info("ep {}: steps={} R={:+.1f} (alive={:+.1f} "
                     "food={:+.1f} water={:+.1f} kills={:+.1f} "
                     "bites={:+.1f} death={:+.1f} vital={:+.1f} "
                     "collect={:+.1f} shelter={:+.1f} craft={:+.1f} "
                     "milestone={:+.1f})",
                     episode_id, s.steps, s.total_reward,
                     s.r_alive, s.r_food, s.r_water, s.r_kills,
                     s.r_bites, s.r_death, s.r_vital,
                     s.r_collect, s.r_shelter, s.r_craft, s.r_milestone);
        if (mf) {
            std::fprintf(mf,
                "{\"episode\":%d,\"steps\":%d,\"reward\":%.3f,"
                "\"r_alive\":%.3f,\"r_food\":%.3f,\"r_water\":%.3f,"
                "\"r_kills\":%.3f,\"r_bites\":%.3f,\"r_death\":%.3f,"
                "\"r_vital\":%.3f,\"r_collect\":%.3f,\"r_shelter\":%.3f,"
                "\"r_craft\":%.3f,\"r_milestone\":%.3f,"
                "\"deaths\":%d,\"policy\":\"%s\",\"seed\":%llu}\n",
                episode_id, s.steps, s.total_reward,
                s.r_alive, s.r_food, s.r_water, s.r_kills,
                s.r_bites, s.r_death, s.r_vital,
                s.r_collect, s.r_shelter, s.r_craft, s.r_milestone,
                env.vitals().deaths, policy.c_str(),
                static_cast<unsigned long long>(seed));
            std::fflush(mf);
        }
    };

    auto t0 = std::chrono::steady_clock::now();
    int steps_done    = 0;
    int episodes_done = 0;

    // Main loop. Two policy paths share the same step/done plumbing.
    while (steps_done < max_steps) {
        dp::agent::AgentAction action;
        std::vector<float> cont_act, disc_act;
        float lp = 0.0f, vv = 0.0f;
        if (policy == "noop") {
            // stand still
        } else if (policy == "wander") {
            action = wander.sample(ecfg.fixed_dt);
        } else if (policy == "ppo") {
            action = ppo->sample_action(obs, cont_act, disc_act, lp, vv);
        } else {
            spdlog::error("Unknown policy '{}'", policy);
            return 1;
        }

        auto sr = env.step(action);
        ++steps_done;

        if (policy == "ppo") {
            dp::policy::Transition tr;
            tr.obs.assign(obs.begin(), obs.end());
            tr.cont_act = std::move(cont_act);
            tr.disc_act = std::move(disc_act);
            tr.reward   = sr.reward;
            tr.value    = vv;
            tr.log_prob = lp;
            tr.done     = sr.done;
            rollout.push_back(std::move(tr));

            if (static_cast<int>(rollout.size()) >= pcfg.rollout_steps) {
                // Bootstrap value for last state if it didn't end in done.
                float last_v = 0.0f;
                if (!rollout.back().done) {
                    std::vector<float> dummy_c, dummy_d;
                    float dummy_lp;
                    ppo->sample_action(sr.obs, dummy_c, dummy_d,
                                       dummy_lp, last_v);
                }
                auto stats = ppo->update(rollout, last_v);
                spdlog::info("PPO update: pi_loss={:+.4f}  v_loss={:.4f}  "
                             "ent={:.3f}  kl={:+.4f}  clip={:.2f}",
                             stats.policy_loss, stats.value_loss,
                             stats.entropy,    stats.approx_kl,
                             stats.clip_frac);
                rollout.clear();
            }
        }

        obs = sr.obs;

        if (sr.done) {
            log_episode(sr.episode_id, env.last_episode_stats());
            ++episodes_done;
            if (max_episodes > 0 && episodes_done >= max_episodes) break;
            env.reset(obs);
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double elapsed_s = std::chrono::duration<double>(t1 - t0).count();
    double steps_per_s = elapsed_s > 0.0 ? steps_done / elapsed_s : 0.0;
    spdlog::info("Done: {} steps, {} episodes, {:.2f}s wall ({:.0f} steps/s)",
                 steps_done, episodes_done, elapsed_s, steps_per_s);

    if (mf) std::fclose(mf);
    return 0;
}
