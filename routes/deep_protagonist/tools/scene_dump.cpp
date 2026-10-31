// deep_protagonist · scene_dump (VIEWER SUPPORT TOOL — not training)
//
// Headless replay dumper. Loads a trained PPO checkpoint, runs ONE
// deterministic episode of the real survival simulation, and writes two
// JSON files that a standalone three.js viewer renders into a full 3D world:
//
//   world.json   — static geometry: heightmap grid, biome grid, rivers,
//                  caves, decorations (trees/rocks/cacti), resource nodes,
//                  plant nodes. Emitted once.
//   frames.json  — per-tick dynamic state: agent pose + vitals + inventory,
//                  animals (kind/state/pos/yaw/health/tamed), campfires,
//                  shelter, construction sites, finished buildings, farm
//                  plots, day/night. Collected-resource / unripe-plant
//                  indices are emitted as small per-frame deltas.
//
// This tool NEVER touches the training loop. It only *reads* the simulation
// state through Environment's const accessors (mirrors main.cpp's renderer
// data-flow, minus raylib). Build target: deep_protagonist_dump.

#include "agent/Observation.hpp"
#include "agent/AgentAction.hpp"
#include "env/Environment.hpp"
#include "policy/PPO.hpp"

#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

dp::world::World::Config load_world_config(const std::string& path) {
    dp::world::World::Config cfg;
    if (!std::filesystem::exists(path)) {
        spdlog::warn("Config '{}' missing; using built-in world defaults.", path);
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

// round to 2 decimals to keep JSON compact
inline double r2(float v) { return std::round((double)v * 100.0) / 100.0; }

void write_world(const std::string& out_dir, const dp::env::Environment& env) {
    const auto& world  = env.world();
    const auto& hm     = world.heightmap();
    const auto& bm     = world.biomes();
    const auto& rivers = world.rivers();
    const auto& caves  = world.caves();
    const auto& decos  = env.decorations().items();
    const auto& res    = env.resources().resources();
    const auto& plants = env.plants().plants();

    std::string path = out_dir + "/world.json";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { spdlog::error("cannot open {}", path); return; }

    const int sx = hm.size_x(), sy = hm.size_y();
    const float cs = hm.cell_scale();
    std::fprintf(f, "{\n");
    std::fprintf(f, "\"grid\":[%d,%d],\"cell_scale\":%.4f,", sx, sy, cs);
    std::fprintf(f, "\"world_size\":[%.2f,%.2f],", hm.world_size_x(), hm.world_size_y());
    std::fprintf(f, "\"max_height\":%.3f,\n", world.config().terrain_max_height);

    // heightmap grid (row-major float meters)
    const auto& h = hm.raw();
    std::fprintf(f, "\"height\":[");
    for (size_t i = 0; i < h.size(); ++i)
        std::fprintf(f, "%s%.2f", i ? "," : "", h[i]);
    std::fprintf(f, "],\n");

    // biome grid as a compact digit string ('0'..'5')
    const auto& b = bm.raw();
    std::fprintf(f, "\"biomes\":\"");
    for (size_t i = 0; i < b.size(); ++i) std::fprintf(f, "%u", (unsigned)b[i]);
    std::fprintf(f, "\",\n");

    // rivers (segments)
    std::fprintf(f, "\"rivers\":[");
    {
        const auto& segs = rivers.segments();
        for (size_t i = 0; i < segs.size(); ++i) {
            const auto& s = segs[i];
            std::fprintf(f, "%s[%.2f,%.2f,%.2f,%.2f]", i ? "," : "",
                         s.x0, s.y0, s.x1, s.y1);
        }
    }
    std::fprintf(f, "],\n");

    // caves (boxes)
    std::fprintf(f, "\"caves\":[");
    {
        const auto& cv = caves.caves();
        for (size_t i = 0; i < cv.size(); ++i) {
            const auto& c = cv[i];
            std::fprintf(f, "%s[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f]", i ? "," : "",
                         c.center_x, c.center_y, c.floor_z, c.ceiling_z,
                         c.half_x, c.half_y);
        }
    }
    std::fprintf(f, "],\n");

    // decorations: [x,y,z,scale,rot,kind]
    std::fprintf(f, "\"decos\":[");
    for (size_t i = 0; i < decos.size(); ++i) {
        const auto& d = decos[i];
        std::fprintf(f, "%s[%.2f,%.2f,%.2f,%.3f,%.3f,%u]", i ? "," : "",
                     d.x, d.y, d.z, d.scale, d.rotation, (unsigned)d.kind);
    }
    std::fprintf(f, "],\n");

    // resources: [x,y,z,kind]   (0=Wood,1=Stone,2=Grass)
    std::fprintf(f, "\"res\":[");
    for (size_t i = 0; i < res.size(); ++i) {
        const auto& r = res[i];
        std::fprintf(f, "%s[%.2f,%.2f,%.2f,%u]", i ? "," : "",
                     r.x, r.y, r.z, (unsigned)r.kind);
    }
    std::fprintf(f, "],\n");

    // plants: [x,y,z,kind]   (0=Berry,1=Fruit,2=Cactus,3=Mushroom)
    std::fprintf(f, "\"plants\":[");
    for (size_t i = 0; i < plants.size(); ++i) {
        const auto& p = plants[i];
        std::fprintf(f, "%s[%.2f,%.2f,%.2f,%u]", i ? "," : "",
                     p.x, p.y, p.z, (unsigned)p.kind);
    }
    std::fprintf(f, "]\n}\n");
    std::fclose(f);
    spdlog::info("wrote {} (grid {}x{}, {} decos, {} res, {} plants, {} rivers, {} caves)",
                 path, sx, sy, decos.size(), res.size(), plants.size(),
                 rivers.segments().size(), caves.caves().size());
}

}  // namespace

int main(int argc, char** argv) {
    std::string load_path, out_dir = "viewer_dump";
    uint64_t seed = 3000000ULL;
    int   max_steps = 9000;
    int   target_frames = 1200;
    bool  nutrition_enabled = false;
    float nutri_protein_decay = 20.0f, nutri_vitamin_decay = 20.0f;
    bool  mask_fire_only = false, mask_atk_fire = false;

    auto next = [&](int& i) { return std::string(argv[++i]); };
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--load")               load_path = next(i);
        else if (a == "--out-dir")            out_dir   = next(i);
        else if (a == "--seed")               seed      = std::stoull(next(i));
        else if (a == "--steps")              max_steps = std::stoi(next(i));
        else if (a == "--frames")             target_frames = std::stoi(next(i));
        else if (a == "--nutrition")          nutrition_enabled = true;
        else if (a == "--nutri-protein-decay")nutri_protein_decay = std::stof(next(i));
        else if (a == "--nutri-vitamin-decay")nutri_vitamin_decay = std::stof(next(i));
        else if (a == "--ppo-mask-fire-only") mask_fire_only = true;
        else if (a == "--ppo-mask-atk-fire")  mask_atk_fire  = true;
    }
    if (load_path.empty()) {
        spdlog::error("usage: deep_protagonist_dump --load ckpt.pt [--seed S] "
                      "[--steps N] [--out-dir DIR] [--nutrition --nutri-protein-decay D --ppo-mask-fire-only]");
        return 2;
    }

    // Build EnvConfig exactly like main_train so the replayed world matches
    // the world the policy was trained/evaluated in.
    dp::env::EnvConfig ecfg;
    ecfg.world      = load_world_config("configs/default.toml");
    ecfg.world.seed = seed;
    ecfg.fixed_dt   = 1.0f / 30.0f;
    ecfg.nutrition_enabled     = nutrition_enabled;
    ecfg.protein_decay_per_min = nutri_protein_decay;
    ecfg.vitamin_decay_per_min = nutri_vitamin_decay;

    std::filesystem::create_directories(out_dir);
    dp::env::Environment env(ecfg);

    dp::agent::ObservationBuilder::Vector obs{};
    env.reset(obs);

    dp::policy::PPOConfig pcfg;
    pcfg.seed = static_cast<int>(seed & 0x7FFFFFFFULL);
    pcfg.n_envs = 1;
    pcfg.mask_fire_only = mask_fire_only;
    pcfg.mask_atk_fire  = mask_atk_fire;
    dp::policy::PPO ppo(pcfg);
    ppo.set_deterministic(true);
    ppo.load(load_path);
    ppo.reset_hidden();
    spdlog::info("loaded checkpoint '{}', seed={}, deterministic eval", load_path, seed);

    // Static world emitted once.
    write_world(out_dir, env);

    const int stride = std::max(1, max_steps / std::max(1, target_frames));

    std::string fpath = out_dir + "/frames.json";
    std::FILE* ff = std::fopen(fpath.c_str(), "wb");
    if (!ff) { spdlog::error("cannot open {}", fpath); return 1; }
    std::fprintf(ff, "{\"stride\":%d,\"fixed_dt\":%.5f,\"frames\":[\n", stride, ecfg.fixed_dt);

    std::vector<float> out_cont; int out_disc; float out_lp, out_val;
    int frames_written = 0;
    bool first = true;

    auto emit_frame = [&](int tick) {
        const auto& st  = env.agent().state();
        const float yaw = env.agent().yaw();
        const auto& v   = env.vitals();
        const auto& day = env.daylight();
        const auto& inv = env.inventory();

        std::fprintf(ff, "%s{", first ? "" : ",\n");
        first = false;
        std::fprintf(ff, "\"tk\":%d,\"t\":%.4f,\"sun\":%.4f,", tick, day.t(),
                     day.sun_height_normalized());
        // agent: x,y,z,yaw, hunger,thirst,stamina,temp, protein,vitamin, alive, cave
        std::fprintf(ff, "\"ag\":[%.2f,%.2f,%.2f,%.3f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%d,%d],",
                     st.pos_x, st.pos_y, st.pos_z, yaw,
                     v.hunger, v.thirst, v.stamina, v.temperature,
                     v.protein, v.vitamin, v.alive ? 1 : 0,
                     env.agent().inside_cave() ? 1 : 0);
        // discrete action id chosen this tick (0..17; 16=NOOP, 17=tend_fire)
        // drives per-action animation + held-tool prop in the 3D viewer.
        std::fprintf(ff, "\"act\":%d,", out_disc);
        // inventory item kinds
        std::fprintf(ff, "\"inv\":[");
        for (int i = 0; i < inv.size(); ++i)
            std::fprintf(ff, "%s%u", i ? "," : "", (unsigned)inv.at(i));
        std::fprintf(ff, "],");
        // animals: [kind,state,x,y,z,yaw,health,tamed]
        std::fprintf(ff, "\"an\":[");
        {
            const auto& as = env.animals().animals();
            bool fa = true;
            for (const auto& a : as) {
                std::fprintf(ff, "%s[%u,%u,%.2f,%.2f,%.2f,%.3f,%.0f,%d]",
                             fa ? "" : ",", (unsigned)a.kind, (unsigned)a.state,
                             a.pos_x, a.pos_y, a.pos_z, a.yaw, a.health,
                             a.is_tamed ? 1 : 0);
                fa = false;
            }
        }
        std::fprintf(ff, "],");
        // campfires: [x,y,z,fuel,lit]
        std::fprintf(ff, "\"fire\":[");
        {
            const auto& fs = env.fires().fires();
            bool fa = true;
            for (const auto& c : fs) {
                float z = env.world().surface_height(c.x, c.y);
                std::fprintf(ff, "%s[%.2f,%.2f,%.2f,%.1f,%d]", fa ? "" : ",",
                             c.x, c.y, z, c.fuel, c.lit ? 1 : 0);
                fa = false;
            }
        }
        std::fprintf(ff, "],");
        // shelter: [x,y,z,r] or []
        {
            const auto& sh = env.shelter();
            if (sh.placed())
                std::fprintf(ff, "\"sh\":[%.2f,%.2f,%.2f,%.2f],",
                             sh.x(), sh.y(), sh.z(), sh.radius());
            else
                std::fprintf(ff, "\"sh\":[],");
        }
        // construction sites: [type,x,y,z,progress,completed]
        std::fprintf(ff, "\"site\":[");
        {
            const auto& ss = env.buildings().sites();
            for (size_t i = 0; i < ss.size(); ++i) {
                const auto& s = ss[i];
                std::fprintf(ff, "%s[%u,%.2f,%.2f,%.2f,%.3f,%d]", i ? "," : "",
                             (unsigned)s.type, s.x, s.y, s.z,
                             s.material_progress(), s.completed ? 1 : 0);
            }
        }
        std::fprintf(ff, "],");
        // finished buildings: [type,x,y,z,coverage]
        std::fprintf(ff, "\"bld\":[");
        {
            const auto& bs = env.buildings().buildings();
            for (size_t i = 0; i < bs.size(); ++i) {
                const auto& b = bs[i];
                std::fprintf(ff, "%s[%u,%.2f,%.2f,%.2f,%.2f]", i ? "," : "",
                             (unsigned)b.type, b.x, b.y, b.z, b.coverage_radius);
            }
        }
        std::fprintf(ff, "],");
        // farm plots: [x,y,z,kind,growth,water,ripe]
        std::fprintf(ff, "\"farm\":[");
        {
            const auto& ps = env.farming().plots();
            for (size_t i = 0; i < ps.size(); ++i) {
                const auto& p = ps[i];
                std::fprintf(ff, "%s[%.2f,%.2f,%.2f,%u,%.3f,%.3f,%d]", i ? "," : "",
                             p.x, p.y, p.z, (unsigned)p.seed_kind, p.growth,
                             p.water_level, p.ripe ? 1 : 0);
            }
        }
        std::fprintf(ff, "],");
        // collected (unavailable) resource indices
        std::fprintf(ff, "\"rgone\":[");
        {
            const auto& rs = env.resources().resources();
            bool fa = true;
            for (size_t i = 0; i < rs.size(); ++i)
                if (!rs[i].available) { std::fprintf(ff, "%s%zu", fa ? "" : ",", i); fa = false; }
        }
        std::fprintf(ff, "],");
        // unripe plant indices
        std::fprintf(ff, "\"pgone\":[");
        {
            const auto& ps = env.plants().plants();
            bool fa = true;
            for (size_t i = 0; i < ps.size(); ++i)
                if (!ps[i].ripe) { std::fprintf(ff, "%s%zu", fa ? "" : ",", i); fa = false; }
        }
        std::fprintf(ff, "]}");
        ++frames_written;
    };

    int tick = 0;
    for (; tick < max_steps; ++tick) {
        // Sample BEFORE emitting so out_disc holds THIS tick's action when the
        // frame is written (emit_frame captures out_disc by reference).
        dp::agent::AgentAction act =
            ppo.sample_action(obs, out_cont, out_disc, out_lp, out_val);
        if (tick % stride == 0) emit_frame(tick);
        dp::env::StepResult res = env.step(act);
        obs = res.obs;
        if (res.done) { emit_frame(tick + 1); break; }
    }

    std::fprintf(ff, "\n]}\n");
    std::fclose(ff);
    spdlog::info("wrote {} ({} frames, {} ticks simulated, stride {})",
                 fpath, frames_written, tick, stride);
    return 0;
}
