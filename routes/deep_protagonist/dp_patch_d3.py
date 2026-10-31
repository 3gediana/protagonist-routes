#!/usr/bin/env python3
# D3 affordance/precondition action mask. Byte-safe (latin-1) edits to the three
# live sources. Gated set: drink(1) [needs water<=6m] and collect(3) [needs an
# available resource<=2m], read from obs with safe margins so a would-succeed
# action is never masked. eat is deliberately NOT gated (hidden cooked-meat
# inventory path is not observable in obs -> gating could block a valid eat).
import sys

PROJ = r"D:\claude-code\c++\routes\deep_protagonist"
HPP  = PROJ + r"\include\policy\PPO.hpp"
CPP  = PROJ + r"\src\policy\PPO.cpp"
MAIN = PROJ + r"\src\main_train.cpp"

def patch(path, edits):
    with open(path, "r", encoding="latin-1") as f:
        s = f.read()
    n0 = len(s)
    for old, new, want in edits:
        c = s.count(old)
        if c != want:
            print(f"FAIL {path}: expected {want} match(es), found {c} for:\n---\n{old[:120]}\n---")
            sys.exit(2)
        s = s.replace(old, new)
    with open(path, "w", encoding="latin-1") as f:
        f.write(s)
    print(f"OK  {path}: {n0} -> {len(s)} bytes (+{len(s)-n0})")

# ---------------- PPO.hpp ----------------
hpp_edits = [
( # H1 config flag
"""    bool   survive_first_mask   = false;
    float  survive_first_thresh = 0.35f;
    // D1: decouple the discrete head from the movement-dominated shared trunk""",
"""    bool   survive_first_mask   = false;
    float  survive_first_thresh = 0.35f;
    // D3: affordance/precondition action mask. Mask a discrete action when the
    // resource its env-handler requires is provably out of reach (read from
    // obs), so the agent cannot waste the step pressing a button with no
    // resource present and is pushed to navigate there first. Default OFF =
    // byte-identical. See apply_afford_mask for the exact gated set + margins.
    bool   afford_mask          = false;
    // D1: decouple the discrete head from the movement-dominated shared trunk""",
1),
( # H2 member
"""    bool                                     survive_first_mask_   = false;
    float                                    survive_first_thresh_ = 0.35f;
    torch::Tensor apply_disc_mask(const torch::Tensor& dl) const {""",
"""    bool                                     survive_first_mask_   = false;
    float                                    survive_first_thresh_ = 0.35f;
    bool                                     afford_mask_          = false;
    torch::Tensor apply_disc_mask(const torch::Tensor& dl) const {""",
1),
( # H3 new method before struct close
"""        auto m      = torch::where(cond, torch::full_like(dl, -50.0f),
                                         torch::zeros_like(dl));            // [B,C]
        return dl + m;                                                      // same-shape add
    }
};""",
"""        auto m      = torch::where(cond, torch::full_like(dl, -50.0f),
                                         torch::zeros_like(dl));            // [B,C]
        return dl + m;                                                      // same-shape add
    }

    // D3 affordance/precondition mask. For a subset of discrete actions whose
    // env-handler requires a nearby resource, add -50 to that action's logit on
    // any row where the resource is provably out of reach (read straight from
    // obs), using a SAFE margin so an action that WOULD succeed is never masked:
    //   drink(1): env drinks within 6 m of River/Swamp; obs water dist_norm
    //             (col 147 = dist/VISION, VISION=60 m; sentinel 1.0 = none).
    //             Mask when dist_norm > 0.22 (~13 m >> 6 m).
    //   collect(3): env collects within 2 m of an available resource; nearest
    //             resource slot0 = dx,dy (cols 99,100, normalised by VISION) +
    //             3-way kind one-hot (cols 102..104; sum 0 = none). Mask when no
    //             resource in vision OR its dist_norm > 0.12 (~7 m >> 2 m).
    // Movement (continuous head) is never masked, so the agent can always still
    // navigate toward the resource. Functional ops + same-shape add (CUDA-safe,
    // identical idiom to apply_disc_mask). Applied at every call site AFTER the
    // survive-first mask so the importance ratio stays consistent. OFF => dl.
    torch::Tensor apply_afford_mask(const torch::Tensor& dl,
                                    const torch::Tensor& obs) const {
        if (!afford_mask_) return dl;
        const int64_t B = dl.size(0);
        auto water_dist  = obs.narrow(1, 147, 1);                           // [B,1]
        auto drink_block = (water_dist > 0.22);                             // [B,1]
        auto res_dx      = obs.narrow(1, 99, 1);
        auto res_dy      = obs.narrow(1, 100, 1);
        auto res_kind    = obs.narrow(1, 102, 1) + obs.narrow(1, 103, 1)
                         + obs.narrow(1, 104, 1);                           // [B,1]
        auto res_dist    = (res_dx * res_dx + res_dy * res_dy).sqrt();      // [B,1] norm
        auto coll_block  = (res_kind < 0.5) | (res_dist > 0.12);            // [B,1]
        auto col      = torch::arange(dl.size(1), dl.options()).unsqueeze(0); // [1,C]
        auto is_drink = (col == 1.0);                                       // [1,C]
        auto is_coll  = (col == 3.0);                                       // [1,C]
        auto cond = (drink_block.expand({B, dl.size(1)})
                     & is_drink.expand({B, dl.size(1)}))
                  | (coll_block.expand({B, dl.size(1)})
                     & is_coll.expand({B, dl.size(1)}));                    // [B,C]
        auto m    = torch::where(cond, torch::full_like(dl, -50.0f),
                                       torch::zeros_like(dl));              // [B,C]
        return dl + m;                                                      // same-shape add
    }
};""",
1),
]

# ---------------- PPO.cpp ----------------
cpp_edits = [
( # C1 ctor cache + log
"""                     survive_first_thresh_);

    spdlog::info("PPO init on device={}, params={}, obs_dim={}",""",
"""                     survive_first_thresh_);

    afford_mask_ = cfg_.afford_mask;
    if (afford_mask_)
        spdlog::info("D3 affordance mask ON: drink(1) masked unless water within "
                     "~13m (obs[147]<0.22; env drinks <=6m); collect(3) masked "
                     "unless an available resource within ~7m (obs[99,100] dist, "
                     "kind obs[102..104]); movement + other actions unaffected "
                     "(policy still free to navigate to the resource)");

    spdlog::info("PPO init on device={}, params={}, obs_dim={}",""",
1),
( # C2 Site A (sample + sample_batch), identical line x2
"    auto dlm         = apply_survive_first_mask(apply_disc_mask(dl), t_obs);",
"    auto dlm         = apply_afford_mask(apply_survive_first_mask(apply_disc_mask(dl), t_obs), t_obs);",
2),
( # C3 Site B (PPO update)
"""            auto dl_disc = apply_disc_mask(dl0);
            auto dl   = apply_survive_first_mask(dl_disc, b_obs);""",
"""            auto dl_disc = apply_disc_mask(dl0);
            auto dl   = apply_afford_mask(apply_survive_first_mask(dl_disc, b_obs), b_obs);""",
1),
( # C4 Site C (KL-ref)
"""                    r_dl = apply_survive_first_mask(
                               apply_disc_mask(std::get<2>(rt)), b_obs);""",
"""                    r_dl = apply_afford_mask(apply_survive_first_mask(
                               apply_disc_mask(std::get<2>(rt)), b_obs), b_obs);""",
1),
]

# ---------------- main_train.cpp ----------------
main_edits = [
( # M1 CLI var
"""    bool        ppo_disc_mem_ctx         = false; // D1.5 disc trunk reads cat(obs,h.detach())
    // D-122 one-pot tech-tree curriculum gates (default OFF = reserved class""",
"""    bool        ppo_disc_mem_ctx         = false; // D1.5 disc trunk reads cat(obs,h.detach())
    bool        ppo_afford_mask          = false; // D3 affordance/precondition mask (drink/collect)
    // D-122 one-pot tech-tree curriculum gates (default OFF = reserved class""",
1),
( # M2 parse
"""        else if (a == "--disc-mem-ctx")        ppo_disc_mem_ctx         = true;
        else if (a == "--allow-cook")          ppo_allow_cook       = true;""",
"""        else if (a == "--disc-mem-ctx")        ppo_disc_mem_ctx         = true;
        else if (a == "--afford-mask")         ppo_afford_mask          = true;
        else if (a == "--allow-cook")          ppo_allow_cook       = true;""",
1),
( # M3 pcfg wire
"""        pcfg.disc_mem_ctx         = ppo_disc_mem_ctx;
        pcfg.kl_ref_coef    = ppo_kl_coef;""",
"""        pcfg.disc_mem_ctx         = ppo_disc_mem_ctx;
        pcfg.afford_mask          = ppo_afford_mask;
        pcfg.kl_ref_coef    = ppo_kl_coef;""",
1),
]

patch(HPP,  hpp_edits)
patch(CPP,  cpp_edits)
patch(MAIN, main_edits)
print("D3 patch applied.")
