#!/usr/bin/env python3
# D2 navigation-survival PBRS: need-gated proximity to water+food, behind a new
# --nav-potential <coef> flag, wired exactly like the existing phi_* shaping.
# Byte-safe (latin-1 round-trip). Each replacement must apply exactly once.
import sys, os
DP = r"D:\claude-code\c++\routes\deep_protagonist"
mt = os.path.join(DP, r"src\main_train.cpp")

repls = [
# E1: phi_nav_survival function, inserted between phi_warmth and int main
(
"""    return std::clamp(obs[IDX_TEMP] / 0.5f, 0.0f, 1.0f);  // 1 @>=neutral -> 0 @freezing
}
int main(int argc, char** argv) {""",
"""    return std::clamp(obs[IDX_TEMP] / 0.5f, 0.0f, 1.0f);  // 1 @>=neutral -> 0 @freezing
}

// D2 navigation-survival PBRS. The one wall no intervention (D1/D1.5/survive-
// first mask) moved across two seed batches is food/thirst DEATH: the agent
// presses eat/drink but dies anyway because it is not NEAR water/food. The
// existing phi_day_forage covers food only, is day-gated, and is ungated by
// need; there is NO water-proximity potential at all. This strict potential-
// based shaping (Ng/Harada/Russell 1999; policy-invariant) is NEED-GATED and
// time-agnostic: Phi = hunger_deficit*food_prox + thirst_deficit*water_prox, so
// the pull toward a resource scales with how badly the matching vital is needed
// and VANISHES when satiated (no Goodhart: a fed agent feels no nav pull). It
// adds the missing WATER axis (obs 145..147; sentinel dist=1 => prox 0) and
// reuses the plant0 anchor (39/40 dx/dy, kind one-hot 42..45) for food.
// Phi in [0,2]. Not a direct eat/drink reward (that is the food/water bucket).
static float phi_nav_survival(const dp::agent::ObservationBuilder::Vector& obs) {
    constexpr int   IDX_HUNGER     = 0;    // satiation /100 in [0,1] (1=full)
    constexpr int   IDX_THIRST     = 1;
    constexpr int   IDX_PLANT0_DX  = 39;
    constexpr int   IDX_PLANT0_DY  = 40;
    constexpr int   IDX_PLANT0_K0  = 42;   // ripe-plant kind one-hot 42..45
    constexpr int   IDX_WATER_DIST = 147;  // nearest water dist/VISION (1=none)
    constexpr float VISION_RANGE   = 60.0f;
    const float hunger_def = std::clamp(1.0f - obs[IDX_HUNGER], 0.0f, 1.0f);
    const float thirst_def = std::clamp(1.0f - obs[IDX_THIRST], 0.0f, 1.0f);
    float food_prox = 0.0f;                 // 0 when no ripe plant in vision
    const float kindsum = obs[IDX_PLANT0_K0] + obs[IDX_PLANT0_K0 + 1]
                        + obs[IDX_PLANT0_K0 + 2] + obs[IDX_PLANT0_K0 + 3];
    if (kindsum >= 0.5f) {
        const float dx = obs[IDX_PLANT0_DX], dy = obs[IDX_PLANT0_DY];
        const float d_m = std::sqrt(dx * dx + dy * dy) * VISION_RANGE;
        food_prox = std::clamp((VISION_RANGE - d_m) / VISION_RANGE, 0.0f, 1.0f);
    }
    // water proximity: sentinel dist_norm=1.0 ("no water in sight") -> prox 0.
    const float water_prox = std::clamp(1.0f - obs[IDX_WATER_DIST], 0.0f, 1.0f);
    return hunger_def * food_prox + thirst_def * water_prox;   // [0,2]
}
int main(int argc, char** argv) {"""
),
# E2: CLI variable declaration
(
"""    float       ppo_warmth_shaping = 0.0f;
    // decision_blueprint_nutrition (round_21): dual nutrition bars + hunting.""",
"""    float       ppo_warmth_shaping = 0.0f;
    // D2 navigation-survival PBRS coef (need-gated proximity to water+food).
    // 0 = off (default). See phi_nav_survival above. Targets the food/thirst
    // death wall that D1/D1.5/survive-first mask did not move.
    float       ppo_nav_shaping = 0.0f;
    // decision_blueprint_nutrition (round_21): dual nutrition bars + hunting."""
),
# E3: CLI parse
(
"""        else if (a == "--ppo-warmth-shaping")     ppo_warmth_shaping     = std::stof(next());""",
"""        else if (a == "--ppo-warmth-shaping")     ppo_warmth_shaping     = std::stof(next());
        else if (a == "--nav-potential")          ppo_nav_shaping        = std::stof(next());"""
),
# E4: startup log
(
"""        pcfg.allow_monument   = ppo_allow_monument;
        ppo = std::make_unique<dp::policy::PPO>(pcfg);""",
"""        pcfg.allow_monument   = ppo_allow_monument;
        if (ppo_nav_shaping > 0.0f)
            spdlog::info("D2 nav-potential PBRS ON (coef {}): need-gated "
                         "proximity to water+food, F=gamma*Phi(s')-Phi(s), "
                         "Phi=hunger_def*food_prox+thirst_def*water_prox "
                         "(policy-invariant; targets the food/thirst death wall)",
                         ppo_nav_shaping);
        ppo = std::make_unique<dp::policy::PPO>(pcfg);"""
),
# E5: VecEnv path injection
(
"""                        shaped_r += ppo_warmth_shaping
                                  * (pcfg.gamma * wphi_sp - wphi_s);
                    }
                    tr.reward   = shaped_r;""",
"""                        shaped_r += ppo_warmth_shaping
                                  * (pcfg.gamma * wphi_sp - wphi_s);
                    }
                    if (ppo_nav_shaping > 0.0f) {
                        float nphi_s  = phi_nav_survival(obs_batch[i]);
                        float nphi_sp = v_sr[i].done ? 0.0f
                                       : phi_nav_survival(v_sr[i].obs);
                        shaped_r += ppo_nav_shaping
                                  * (pcfg.gamma * nphi_sp - nphi_s);
                    }
                    tr.reward   = shaped_r;"""
),
# E6: single-env path injection
(
"""                shaped_r += ppo_warmth_shaping * (pcfg.gamma * wphi_sp - wphi_s);
            }
            tr.reward   = shaped_r;""",
"""                shaped_r += ppo_warmth_shaping * (pcfg.gamma * wphi_sp - wphi_s);
            }
            if (ppo_nav_shaping > 0.0f) {
                float nphi_s  = phi_nav_survival(obs);
                float nphi_sp = sr.done ? 0.0f : phi_nav_survival(sr.obs);
                shaped_r += ppo_nav_shaping * (pcfg.gamma * nphi_sp - nphi_s);
            }
            tr.reward   = shaped_r;"""
),
]

with open(mt, "rb") as f:
    raw = f.read()
s = raw.decode("latin-1")
for i, (old, new) in enumerate(repls):
    n = s.count(old)
    if n != 1:
        print(f"FAIL repl#{i}: expected 1 match, found {n}")
        sys.exit(2)
    s = s.replace(old, new)
out = s.encode("latin-1")
with open(mt, "wb") as f:
    f.write(out)
print(f"OK {mt}: {len(repls)} replacements, {len(raw)} -> {len(out)} bytes")
print("D2 PATCH APPLIED")
