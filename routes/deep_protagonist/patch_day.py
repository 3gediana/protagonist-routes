#!/usr/bin/env python3
# round_19 Path B: add phi_day_forage daytime-foraging policy-invariant
# potential + --ppo-day-shaping CLI + inject at both PPO transition sites.
# Binary mode, CRLF-preserving (per prior CRLF-corruption lesson).
import sys

path = sys.argv[1]
b = open(path, "rb").read()
crlf_before = b.count(b"\r\n")

def rep(buf, old, new, label, n=1):
    c = buf.count(old)
    assert c == n, f"[{label}] expected {n} match, found {c}"
    return buf.replace(old, new)

NL = b"\r\n"

# --- 1. insert phi_day_forage after phi_night_shelter ---
anchor1 = (b"    return std::clamp((20.0f - d_m) / 12.0f, 0.0f, 1.0f);  // 1 @<=8m -> 0 @20m" + NL
           + b"}" + NL)
phi_day = NL.join([
    b"}",
    b"",
    b"// round_19 Path B: daytime foraging potential to offset the new-bank",
    b"// food-death side-effect. Policy-invariant when summed with phi_night",
    b"// (Ng 1999 / Wiewiora 2003): Phi_total = Phi_night + Phi_day. This is",
    b"// NOT a direct food/collect reward (that would be Goodhart). Phi in",
    b"// [0,1]: by day (sun_h>+0.1), graded by proximity to the nearest ripe",
    b"// plant (obs 39..40 dx/dy, kind one-hot 42..45); 0 at night and when",
    b"// no ripe plant is in vision.",
    b"static float phi_day_forage(const dp::agent::ObservationBuilder::Vector& obs) {",
    b"    constexpr int   IDX_DAY_COS   = 13;",
    b"    constexpr int   IDX_PLANT0_DX = 39;",
    b"    constexpr int   IDX_PLANT0_DY = 40;",
    b"    constexpr int   IDX_PLANT0_K0 = 42;   // one-hot kind occupies 42..45",
    b"    constexpr float VISION_RANGE  = 60.0f;",
    b"    const float sun_h = -obs[IDX_DAY_COS];          // sun_height_normalized()",
    b"    if (sun_h <= 0.1f) return 0.0f;                 // not clearly daytime",
    b"    const float kindsum = obs[IDX_PLANT0_K0] + obs[IDX_PLANT0_K0 + 1]",
    b"                        + obs[IDX_PLANT0_K0 + 2] + obs[IDX_PLANT0_K0 + 3];",
    b"    if (kindsum < 0.5f) return 0.0f;                // no ripe plant in vision",
    b"    const float dx = obs[IDX_PLANT0_DX], dy = obs[IDX_PLANT0_DY];",
    b"    const float d_m = std::sqrt(dx * dx + dy * dy) * VISION_RANGE;",
    b"    return std::clamp((VISION_RANGE - d_m) / VISION_RANGE, 0.0f, 1.0f); // 1 @0m -> 0 @60m",
    b"}",
    b"",
]) + NL
new1 = (b"    return std::clamp((20.0f - d_m) / 12.0f, 0.0f, 1.0f);  // 1 @<=8m -> 0 @20m" + NL
        + phi_day)
b = rep(b, anchor1, new1, "phi_day_forage def")

# --- 2. CLI variable ---
b = rep(b,
    b"    float       ppo_night_shaping = 0.0f; // decision_round_16 Step2: coef for night-shelter PBRS (0=off)" + NL,
    b"    float       ppo_night_shaping = 0.0f; // decision_round_16 Step2: coef for night-shelter PBRS (0=off)" + NL
    + b"    float       ppo_day_shaping   = 0.0f; // round_19 Path B: coef for daytime-forage PBRS (0=off)" + NL,
    "cli var")

# --- 3. CLI parse ---
b = rep(b,
    b'        else if (a == "--ppo-night-shaping") ppo_night_shaping = std::stof(next());' + NL,
    b'        else if (a == "--ppo-night-shaping") ppo_night_shaping = std::stof(next());' + NL
    + b'        else if (a == "--ppo-day-shaping")   ppo_day_shaping   = std::stof(next());' + NL,
    "cli parse")

# --- 4. injection site 1 (VecEnv batch) ---
site1_old = (
    b"                    if (ppo_night_shaping > 0.0f) {" + NL +
    b"                        float phi_s  = phi_night_shelter(obs_batch[i]);" + NL +
    b"                        float phi_sp = v_sr[i].done ? 0.0f" + NL +
    b"                                       : phi_night_shelter(v_sr[i].obs);" + NL +
    b"                        shaped_r += ppo_night_shaping" + NL +
    b"                                  * (pcfg.gamma * phi_sp - phi_s);" + NL +
    b"                    }" + NL)
site1_add = (
    b"                    if (ppo_day_shaping > 0.0f) {" + NL +
    b"                        float dphi_s  = phi_day_forage(obs_batch[i]);" + NL +
    b"                        float dphi_sp = v_sr[i].done ? 0.0f" + NL +
    b"                                       : phi_day_forage(v_sr[i].obs);" + NL +
    b"                        shaped_r += ppo_day_shaping" + NL +
    b"                                  * (pcfg.gamma * dphi_sp - dphi_s);" + NL +
    b"                    }" + NL)
b = rep(b, site1_old, site1_old + site1_add, "inject site1")

# --- 5. injection site 2 (single-env) ---
site2_old = (
    b"            if (ppo_night_shaping > 0.0f) {" + NL +
    b"                float phi_s  = phi_night_shelter(obs);" + NL +
    b"                float phi_sp = sr.done ? 0.0f : phi_night_shelter(sr.obs);" + NL +
    b"                shaped_r += ppo_night_shaping * (pcfg.gamma * phi_sp - phi_s);" + NL +
    b"            }" + NL)
site2_add = (
    b"            if (ppo_day_shaping > 0.0f) {" + NL +
    b"                float dphi_s  = phi_day_forage(obs);" + NL +
    b"                float dphi_sp = sr.done ? 0.0f : phi_day_forage(sr.obs);" + NL +
    b"                shaped_r += ppo_day_shaping * (pcfg.gamma * dphi_sp - dphi_s);" + NL +
    b"            }" + NL)
b = rep(b, site2_old, site2_old + site2_add, "inject site2")

crlf_after = b.count(b"\r\n")
assert b.count(b"\n") == b.count(b"\r\n"), "MIXED line endings detected (lone LF)!"
open(path, "wb").write(b)
print(f"OK patched {path}")
print(f"CRLF before={crlf_before} after={crlf_after} (added {crlf_after-crlf_before} lines)")
print(f"phi_day_forage defs: {b.count(b'phi_day_forage(')}  (expect 5: 1 def + 4 calls)")
print(f"ppo_day_shaping refs: {b.count(b'ppo_day_shaping')}  (expect 5: 1 decl + 1 parse + ... )")
