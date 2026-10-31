#!/usr/bin/env python3
# D1.5 patch: give the decoupled discrete trunk recurrent context via
# cat(obs, h.detach()), behind a new --disc-mem-ctx flag. Byte-safe (latin-1
# round-trip preserves BOM/LF). Each replacement must apply exactly once.
import sys, os

DP = r"D:\claude-code\c++\routes\deep_protagonist"

def patch(path, repls):
    with open(path, "rb") as f:
        raw = f.read()
    s = raw.decode("latin-1")
    for i, (old, new) in enumerate(repls):
        n = s.count(old)
        if n != 1:
            print(f"FAIL {path} repl#{i}: expected 1 match, found {n}")
            sys.exit(2)
        s = s.replace(old, new)
    out = s.encode("latin-1")
    with open(path, "wb") as f:
        f.write(out)
    print(f"OK {path}: {len(repls)} replacements, {len(raw)} -> {len(out)} bytes")

hpp = os.path.join(DP, r"include\policy\PPO.hpp")
cpp = os.path.join(DP, r"src\policy\PPO.cpp")
mt  = os.path.join(DP, r"src\main_train.cpp")

patch(hpp, [
    (
"""    torch::nn::Linear   disc_trunk{nullptr};   // OBS_DIM  -> DISC_HID
    torch::nn::Linear   disc_head2{nullptr};   // DISC_HID -> DISC_CATS (zero-init)
    bool                decouple_disc = false; // set from PPOConfig in PPO ctor""",
"""    torch::nn::Linear   disc_trunk{nullptr};   // OBS_DIM  -> DISC_HID
    // D1.5: dedicated discrete trunk whose input is cat(obs, h.detach()) =
    // OBS_DIM+HIDDEN -> DISC_HID. Used only when disc_mem_ctx is ON. Registered
    // ALWAYS so it serializes / warm-starts tolerantly like disc_trunk.
    torch::nn::Linear   disc_trunk_mem{nullptr}; // OBS_DIM+HIDDEN -> DISC_HID
    torch::nn::Linear   disc_head2{nullptr};   // DISC_HID -> DISC_CATS (zero-init)
    bool                decouple_disc = false; // set from PPOConfig in PPO ctor
    bool                disc_mem_ctx  = false; // set from PPOConfig in PPO ctor"""
    ),
    (
"""    bool   decouple_disc        = false;
    float  kl_ref_coef   = 0.0f;   // beta for KL(pi||pi_ref=bc_v9) anchor (0=off)""",
"""    bool   decouple_disc        = false;
    // D1.5: feed the dedicated discrete trunk cat(obs, h.detach()) instead of
    // obs alone, giving its nonlinear path the shared GRU's recurrent memory
    // (still stop-grad). Only meaningful when decouple_disc is ON.
    bool   disc_mem_ctx         = false;
    float  kl_ref_coef   = 0.0f;   // beta for KL(pi||pi_ref=bc_v9) anchor (0=off)"""
    ),
])

patch(cpp, [
    (
"""    disc_trunk  = register_module("disc_trunk",
                                  torch::nn::Linear(OBS_DIM, DISC_HID));
    disc_head2  = register_module("disc_head2",""",
"""    disc_trunk  = register_module("disc_trunk",
                                  torch::nn::Linear(OBS_DIM, DISC_HID));
    // D1.5: memory-context trunk, input cat(obs, h.detach()) -> DISC_HID.
    disc_trunk_mem = register_module("disc_trunk_mem",
                                  torch::nn::Linear(OBS_DIM + HIDDEN, DISC_HID));
    disc_head2  = register_module("disc_head2","""
    ),
    (
"""    orthogonal_(disc_trunk,  std::sqrt(2.0f));  // D1: hidden trunk, ReLU gain""",
"""    orthogonal_(disc_trunk,  std::sqrt(2.0f));  // D1: hidden trunk, ReLU gain
    orthogonal_(disc_trunk_mem, std::sqrt(2.0f)); // D1.5: same ReLU-gain init"""
    ),
    (
"""    auto dl  = decouple_disc
        ? (disc_logits(h.detach())
           + disc_head2(torch::relu(disc_trunk(obs))))
        : disc_logits(h);                               // [B, DISC_CATS]""",
"""    // D1.5: when disc_mem_ctx, the dedicated trunk reads cat(obs, h.detach())
    // so its nonlinear path can use the shared GRU memory (still stop-grad, so
    // it never reshapes the movement trunk); else legacy obs-only D1 path.
    torch::Tensor dl;
    if (decouple_disc) {
        auto disc_feat = disc_mem_ctx
            ? torch::relu(disc_trunk_mem(torch::cat({obs, h.detach()}, 1)))
            : torch::relu(disc_trunk(obs));
        dl = disc_logits(h.detach()) + disc_head2(disc_feat);
    } else {
        dl = disc_logits(h);                            // [B, DISC_CATS]
    }"""
    ),
    (
"""    net_->decouple_disc = cfg.decouple_disc;
    net_->to(device_);""",
"""    net_->decouple_disc = cfg.decouple_disc;
    net_->disc_mem_ctx  = cfg.disc_mem_ctx;
    net_->to(device_);"""
    ),
    (
"""                     "the discrete-policy gradient", ActorCriticImpl::DISC_HID);""",
"""                     "the discrete-policy gradient", ActorCriticImpl::DISC_HID);
    if (cfg_.decouple_disc && cfg_.disc_mem_ctx)
        spdlog::info("D1.5 disc-mem-ctx ON: dedicated discrete trunk reads "
                     "cat(obs, h.detach()) (input {}+{}={}) so its nonlinear "
                     "path uses the shared GRU memory (stop-grad preserved)",
                     ActorCriticImpl::OBS_DIM, ActorCriticImpl::HIDDEN,
                     ActorCriticImpl::OBS_DIM + ActorCriticImpl::HIDDEN);"""
    ),
])

patch(mt, [
    (
"""    bool        ppo_decouple_disc        = false; // D1 discrete-head decoupling A/B""",
"""    bool        ppo_decouple_disc        = false; // D1 discrete-head decoupling A/B
    bool        ppo_disc_mem_ctx         = false; // D1.5 disc trunk reads cat(obs,h.detach())"""
    ),
    (
"""        else if (a == "--decouple-disc")       ppo_decouple_disc        = true;""",
"""        else if (a == "--decouple-disc")       ppo_decouple_disc        = true;
        else if (a == "--disc-mem-ctx")        ppo_disc_mem_ctx         = true;"""
    ),
    (
"""        pcfg.decouple_disc        = ppo_decouple_disc;""",
"""        pcfg.decouple_disc        = ppo_decouple_disc;
        pcfg.disc_mem_ctx         = ppo_disc_mem_ctx;"""
    ),
])
print("ALL PATCHES APPLIED")
