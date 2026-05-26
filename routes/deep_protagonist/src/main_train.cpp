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

#include <array>
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
// Per-episode action histogram for C-diagnostic. Counts how many ticks
// each of the 16 discrete triggers was set, plus mean |move| and |yaw|.
// Index map (matches AgentAction discrete order):
//   0:eat 1:drink 2:attack 3:collect 4:place_shelter 5:craft_spear 6:sleep
//   7:craft_grass_dress 8:craft_fur_cloak 9:wear_clothes
//   10:place_blueprint 11:cycle_building_type 12:deposit_to_site
//   13:plant_seed 14:water_plot 15:feed_tame
struct ActionStats {
    int disc_count[16] = {0};
    double move_mag_sum = 0.0;
    double yaw_abs_sum  = 0.0;
    int    ticks        = 0;

    void accumulate(const dp::agent::AgentAction& a) {
        disc_count[ 0] += a.eat                 ? 1 : 0;
        disc_count[ 1] += a.drink               ? 1 : 0;
        disc_count[ 2] += a.attack              ? 1 : 0;
        disc_count[ 3] += a.collect             ? 1 : 0;
        disc_count[ 4] += a.place_shelter       ? 1 : 0;
        disc_count[ 5] += a.craft_spear         ? 1 : 0;
        disc_count[ 6] += a.sleep               ? 1 : 0;
        disc_count[ 7] += a.craft_grass_dress   ? 1 : 0;
        disc_count[ 8] += a.craft_fur_cloak     ? 1 : 0;
        disc_count[ 9] += a.wear_clothes        ? 1 : 0;
        disc_count[10] += a.place_blueprint     ? 1 : 0;
        disc_count[11] += a.cycle_building_type ? 1 : 0;
        disc_count[12] += a.deposit_to_site     ? 1 : 0;
        disc_count[13] += a.plant_seed          ? 1 : 0;
        disc_count[14] += a.water_plot          ? 1 : 0;
        disc_count[15] += a.feed_tame           ? 1 : 0;
        move_mag_sum += std::sqrt(a.move_x * a.move_x + a.move_y * a.move_y);
        yaw_abs_sum  += std::fabs(a.yaw_rate);
        ++ticks;
    }

    void reset() {
        for (int i = 0; i < 16; ++i) disc_count[i] = 0;
        move_mag_sum = 0.0;
        yaw_abs_sum  = 0.0;
        ticks        = 0;
    }
};

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
    std::string save_path;                // checkpoint output (PPO only)
    int         save_every = 0;           // save every N updates (0=off)
    std::string load_path;                // resume from this checkpoint
    bool        no_update  = false;       // pure-eval: skip rollout collection and PPO update
    float       thirst_decay = 12.0f;      // D-053: blueprint line 60. D-041 used 8 as middle ground; with D-052 stack in place we need the full 12/min so drink stays a survival need across an episode.
    float       trigger_cost = 0.001f;     // EnvConfig.trigger_cost

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                spdlog::error("missing value for {}", a);
                std::exit(2);
            }
            return std::string(argv[++i]);
        };
        if      (a == "--steps")      max_steps    = std::stoi(next());
        else if (a == "--episodes")   max_episodes = std::stoi(next());
        else if (a == "--seed")       seed         = std::stoull(next());
        else if (a == "--policy")     policy       = next();
        else if (a == "--metrics")    metrics_path = next();
        else if (a == "--save-path")  save_path    = next();
        else if (a == "--save-every") save_every   = std::stoi(next());
        else if (a == "--load")       load_path    = next();
        else if (a == "--no-update")  no_update    = true;
        else if (a == "--thirst-decay") thirst_decay = std::stof(next());
        else if (a == "--trigger-cost") trigger_cost = std::stof(next());
        else if (a == "-h" || a == "--help") {
            std::printf(
                "Usage: %s [--steps N] [--episodes N] [--seed S]\n"
                "       [--policy wander|noop|ppo]\n"
                "       [--metrics path.jsonl]\n"
                "       [--save-path file.pt --save-every N]\n"
                "       [--load file.pt]\n"
                "       [--thirst-decay X --trigger-cost Y]\n",
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
    ecfg.thirst_decay_per_min = thirst_decay;
    ecfg.trigger_cost         = trigger_cost;
    spdlog::info("Env tunables: thirst_decay={}/min, trigger_cost={}/step",
                 thirst_decay, trigger_cost);

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
        if (!load_path.empty()) {
            ppo->load(load_path);
        }
    }
    int updates_since_save = 0;
    int total_updates      = 0;

    ActionStats astats;

    // D-043: Crafter-style cumulative milestone tracking. unlock_total[i] =
    // number of episodes in which milestone i was unlocked. The score is the
    // geometric mean of success rates: exp(mean(log(1 + s_i))) - 1, which
    // favors agents that unlock many milestones over those that grind one.
    std::array<int, dp::env::EpisodeMilestones::N> unlock_total{};
    int score_eps = 0;
    auto compute_score = [&]() {
        if (score_eps == 0) return 0.0;
        double s = 0.0;
        for (int i = 0; i < dp::env::EpisodeMilestones::N; ++i) {
            double rate = static_cast<double>(unlock_total[i]) / score_eps;
            s += std::log(1.0 + rate);
        }
        return std::exp(s / dp::env::EpisodeMilestones::N) - 1.0;
    };

    auto log_episode = [&](int episode_id,
                           const dp::agent::RewardSystem::EpisodeStats& s,
                           const dp::env::EpisodeMilestones& ms) {
        // Update cumulative counters BEFORE computing the score.
        for (int i = 0; i < dp::env::EpisodeMilestones::N; ++i) {
            if (ms.get(i)) ++unlock_total[i];
        }
        ++score_eps;
        double score = compute_score();
        spdlog::info("ep {}: steps={} R={:+.1f} unlocks={}/{} score={:.3f} "
                     "(alive={:+.1f} food={:+.1f} water={:+.1f} kills={:+.1f} "
                     "bites={:+.1f} death={:+.1f} vital={:+.1f} "
                     "collect={:+.1f} shelter={:+.1f} craft={:+.1f} "
                     "milestone={:+.1f} potential={:+.1f})",
                     episode_id, s.steps, s.total_reward,
                     ms.count(), dp::env::EpisodeMilestones::N, score,
                     s.r_alive, s.r_food, s.r_water, s.r_kills,
                     s.r_bites, s.r_death, s.r_vital,
                     s.r_collect, s.r_shelter, s.r_craft, s.r_milestone,
                     s.r_potential);
        if (mf) {
            double mv = astats.ticks > 0 ? astats.move_mag_sum / astats.ticks : 0.0;
            double yw = astats.ticks > 0 ? astats.yaw_abs_sum  / astats.ticks : 0.0;
            std::fprintf(mf,
                "{\"episode\":%d,\"steps\":%d,\"reward\":%.3f,"
                "\"r_alive\":%.3f,\"r_food\":%.3f,\"r_water\":%.3f,"
                "\"r_kills\":%.3f,\"r_bites\":%.3f,\"r_death\":%.3f,"
                "\"r_vital\":%.3f,\"r_collect\":%.3f,\"r_shelter\":%.3f,"
                "\"r_craft\":%.3f,\"r_milestone\":%.3f,\"r_potential\":%.3f,"
                "\"deaths\":%d,\"policy\":\"%s\",\"seed\":%llu,"
                "\"mv\":%.3f,\"yw\":%.3f,\"score\":%.4f,\"unlocks\":[",
                episode_id, s.steps, s.total_reward,
                s.r_alive, s.r_food, s.r_water, s.r_kills,
                s.r_bites, s.r_death, s.r_vital,
                s.r_collect, s.r_shelter, s.r_craft, s.r_milestone,
                s.r_potential,
                env.vitals().deaths, policy.c_str(),
                static_cast<unsigned long long>(seed),
                mv, yw, score);
            for (int i = 0; i < dp::env::EpisodeMilestones::N; ++i) {
                std::fprintf(mf, "%s%d", i == 0 ? "" : ",", ms.get(i) ? 1 : 0);
            }
            std::fprintf(mf, "],\"act\":[");
            for (int i = 0; i < 16; ++i) {
                std::fprintf(mf, "%s%d", i == 0 ? "" : ",", astats.disc_count[i]);
            }
            std::fprintf(mf, "]}\n");
            std::fflush(mf);
        }
        astats.reset();
    };

    auto t0 = std::chrono::steady_clock::now();
    int steps_done    = 0;
    int episodes_done = 0;

    // Main loop. Two policy paths share the same step/done plumbing.
    std::vector<float> ppo_h_in_snapshot;   // h_in for current step
    while (steps_done < max_steps) {
        dp::agent::AgentAction action;
        std::vector<float> cont_act;
        int  disc_idx = dp::policy::ActorCriticImpl::NOOP_IDX;
        float lp = 0.0f, vv = 0.0f;
        if (policy == "noop") {
            // stand still
        } else if (policy == "wander") {
            action = wander.sample(ecfg.fixed_dt);
        } else if (policy == "ppo") {
            // Snapshot the GRU hidden BEFORE sample_action mutates it.
            // h_in is the value PPO::update replays during the recurrent
            // forward pass, so it has to match the hidden the GRU saw
            // on entry to this step.
            ppo_h_in_snapshot = ppo->current_hidden();
            action = ppo->sample_action(obs, cont_act, disc_idx, lp, vv);
        } else {
            spdlog::error("Unknown policy '{}'", policy);
            return 1;
        }

        astats.accumulate(action);
        auto sr = env.step(action);
        ++steps_done;

        if (policy == "ppo" && !no_update) {
            dp::policy::Transition tr;
            tr.obs.assign(obs.begin(), obs.end());
            tr.cont_act = std::move(cont_act);
            tr.disc_act = disc_idx;
            tr.h_in     = std::move(ppo_h_in_snapshot);
            tr.reward   = sr.reward;
            tr.value    = vv;
            tr.log_prob = lp;
            tr.done     = sr.done;
            rollout.push_back(std::move(tr));

            if (static_cast<int>(rollout.size()) >= pcfg.rollout_steps) {
                // Bootstrap value for last state if it didn't end in done.
                float last_v = 0.0f;
                if (!rollout.back().done) {
                    std::vector<float> dummy_c;
                    int   dummy_idx = 0;
                    float dummy_lp;
                    ppo->sample_action(sr.obs, dummy_c, dummy_idx,
                                       dummy_lp, last_v);
                }
                auto stats = ppo->update(rollout, last_v);
                ++total_updates;
                ++updates_since_save;
                spdlog::info("PPO update #{:3d}: pi={:+.4f}  v={:.4f}  "
                             "ent={:.3f}  kl={:+.4f}  clip={:.2f}",
                             total_updates,
                             stats.policy_loss, stats.value_loss,
                             stats.entropy,    stats.approx_kl,
                             stats.clip_frac);
                rollout.clear();

                if (save_every > 0 && !save_path.empty()
                    && updates_since_save >= save_every) {
                    ppo->save(save_path);
                    updates_since_save = 0;
                }
            }
        }

        obs = sr.obs;

        if (sr.done) {
            // D-043: snapshot milestones BEFORE reset() clears the flags.
            const auto ms = env.milestones();
            log_episode(sr.episode_id, env.last_episode_stats(), ms);
            ++episodes_done;
            if (max_episodes > 0 && episodes_done >= max_episodes) break;
            env.reset(obs);
            // New life -> reset GRU hidden so memory doesn't bleed across
            // episodes (matches reset of SpatialMemory inside Environment).
            if (policy == "ppo") ppo->reset_hidden();
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double elapsed_s = std::chrono::duration<double>(t1 - t0).count();
    double steps_per_s = elapsed_s > 0.0 ? steps_done / elapsed_s : 0.0;
    spdlog::info("Done: {} steps, {} episodes, {:.2f}s wall ({:.0f} steps/s)",
                 steps_done, episodes_done, elapsed_s, steps_per_s);

    // Final save - always write a checkpoint at the end if save_path was
    // provided, regardless of the save_every cadence.
    if (policy == "ppo" && ppo && !save_path.empty()) {
        ppo->save(save_path);
    }
    if (mf) std::fclose(mf);
    return 0;
}
