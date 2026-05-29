#pragma once

// D-101: Behavioral Cloning pretrainer.
//
// Trains the SAME ActorCritic network used by PPO to imitate the
// ScriptedSettler oracle (D-100). PPO proved unable to self-discover
// settlement across 8 env-side attempts (D-091..D-099) because the
// single-step foraging attractor (+5 r_food/min while wandering)
// dominates the long-horizon shelter payoff. The fix is to give PPO a
// starting solution: clone the scripted policy's obs->action mapping,
// then PPO-finetune from that checkpoint so the network keeps the
// settlement behaviour while re-optimising the rest under real reward.
//
// Demo file format (little-endian, written by main_train --record-demos):
//   magic    : 4 bytes  "DPB1"
//   version  : uint32 = 1
//   obs_dim  : uint32
//   cont_dim : uint32
//   n_rec    : uint64   (patched on close; 0 while streaming)
//   records  : n_rec * {
//       obs_dim  float32   (observation the policy saw this tick)
//       cont_dim float32   (PRE-tanh continuous target = atanh(move/yaw))
//       uint32             (discrete category index, 0..16; 16 = NOOP)
//       uint32             (done flag: 1 on the last tick of an episode)
//   }
//
// The done flags delimit episodes so BC can replay each episode as a
// sequence with the GRU hidden state carried forward (truncated BPTT),
// matching exactly how the net is driven at inference / PPO time. This
// avoids the train/test mismatch a memoryless (h=0) clone would create.

#include "policy/PPO.hpp"

#include <spdlog/spdlog.h>
#include <torch/torch.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace dp::policy {

struct BCConfig {
    std::vector<std::string> demo_paths;
    std::string out_path        = "bc.pt";
    int   epochs                = 8;
    int   batch_eps             = 16;     // episodes replayed in parallel (batch dim)
    int   bptt                  = 64;     // truncated-BPTT window length
    float lr                    = 1e-3f;
    float trigger_weight        = 5.0f;   // CE class weight for the 16 non-NOOP triggers
    int   min_eplen             = 0;      // drop episodes shorter than this (instant-death seeds)
    int   max_eplen             = 0;      // 0 = no cap; else keep only first N steps of each ep
                                          // (the oracle deposits/builds early then idles for
                                          //  thousands of NOOP steps; trimming the idle tail
                                          //  stops BC collapsing to always-NOOP).
    int   build_oversample      = 0;      // add this many extra short "build-phase" episodes per
                                          // real episode. The oracle deposits twice in the first
                                          // ~35 steps then idles thousands of NOOP steps, so the
                                          // rare deposit is drowned. Replaying the first build_len
                                          // steps build_oversample times rebalances the data so the
                                          // clone learns BOTH to deposit/build AND to camp.
    int   build_len             = 50;     // length of an oversampled build-phase snippet
    bool  use_cuda              = true;
    int   seed                  = 1234;
};

class BCTrainer {
public:
    explicit BCTrainer(const BCConfig& cfg) : cfg_(cfg) {}

    // Returns true on success.
    bool run() {
        constexpr int O = ActorCriticImpl::OBS_DIM;
        constexpr int C = ActorCriticImpl::CONT_DIM;
        constexpr int CATS = ActorCriticImpl::DISC_CATS;
        constexpr int H = ActorCriticImpl::HIDDEN;

        if (!load_demos(O, C)) return false;
        if (episodes_.empty()) {
            spdlog::error("BC: no episodes parsed from demos.");
            return false;
        }
        spdlog::info("BC: loaded {} records across {} episodes "
                     "(dropped {} short eps < {} steps; obs_dim={}, cont_dim={})",
                     n_rec_, episodes_.size(), dropped_eps_, cfg_.min_eplen, O, C);

        torch::Device device((cfg_.use_cuda && torch::cuda::is_available())
                                 ? torch::Device(torch::kCUDA, 0)
                                 : torch::Device(torch::kCPU));
        torch::manual_seed(cfg_.seed);

        ActorCritic net;
        net->to(device);
        torch::optim::Adam opt(net->parameters(),
                               torch::optim::AdamOptions(cfg_.lr));

        // Per-class CE weights: NOOP (idx 16) = 1, the 16 triggers upweighted
        // so survival-critical but sparse actions (drink/eat/deposit) are not
        // drowned out by the NOOP majority.
        std::vector<float> wvec(CATS, cfg_.trigger_weight);
        wvec[ActorCriticImpl::NOOP_IDX] = 1.0f;
        auto class_w = torch::from_blob(wvec.data(), {CATS}, torch::kFloat32)
                           .clone().to(device);

        // Sort episodes by length (desc) so each parallel batch has similar
        // lengths -> minimal padding waste.
        std::vector<int> order(episodes_.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            return episodes_[a].len > episodes_[b].len;
        });

        std::mt19937 rng(static_cast<uint32_t>(cfg_.seed));

        // Partition the length-sorted order into fixed batch_eps-sized groups.
        // Each group is length-homogeneous (minimal padding). We shuffle the
        // ORDER of groups each epoch (not episodes across groups) so heavy
        // 9000-step full episodes never share a batch with 50-step build
        // snippets, which would pad every snippet out to 9000 steps.
        std::vector<size_t> group_starts;
        for (size_t g0 = 0; g0 < order.size(); g0 += cfg_.batch_eps)
            group_starts.push_back(g0);

        for (int ep = 0; ep < cfg_.epochs; ++ep) {
            if (ep > 0) std::shuffle(group_starts.begin(), group_starts.end(), rng);

            double epoch_cont = 0.0, epoch_disc = 0.0;
            long   epoch_steps = 0;
            int    n_groups = 0;

            for (size_t gi = 0; gi < group_starts.size(); ++gi) {
                size_t g0 = group_starts[gi];
                size_t g1 = std::min(order.size(),
                                     g0 + static_cast<size_t>(cfg_.batch_eps));
                int B = static_cast<int>(g1 - g0);
                int maxT = 0;
                for (size_t i = g0; i < g1; ++i)
                    maxT = std::max(maxT, episodes_[order[i]].len);
                if (maxT == 0) continue;

                // Build host [maxT, B, *] dense tensors with a validity mask.
                std::vector<float>   h_obs (static_cast<size_t>(maxT) * B * O, 0.0f);
                std::vector<float>   h_cont(static_cast<size_t>(maxT) * B * C, 0.0f);
                std::vector<int64_t> h_disc(static_cast<size_t>(maxT) * B,
                                            ActorCriticImpl::NOOP_IDX);
                std::vector<float>   h_mask(static_cast<size_t>(maxT) * B, 0.0f);

                for (int b = 0; b < B; ++b) {
                    const Episode& e = episodes_[order[g0 + b]];
                    for (int t = 0; t < e.len; ++t) {
                        size_t src = static_cast<size_t>(e.start + t);
                        size_t dst = (static_cast<size_t>(t) * B + b);
                        std::copy(obs_.begin()  + src * O,
                                  obs_.begin()  + src * O + O,
                                  h_obs.begin()  + dst * O);
                        std::copy(cont_.begin() + src * C,
                                  cont_.begin() + src * C + C,
                                  h_cont.begin() + dst * C);
                        h_disc[dst] = disc_[src];
                        h_mask[dst] = 1.0f;
                    }
                }

                auto t_obs  = torch::from_blob(h_obs.data(),  {maxT, B, O}, torch::kFloat32).clone();
                auto t_cont = torch::from_blob(h_cont.data(), {maxT, B, C}, torch::kFloat32).clone();
                auto t_disc = torch::from_blob(h_disc.data(), {maxT, B},    torch::kInt64).clone();
                auto t_mask = torch::from_blob(h_mask.data(), {maxT, B},    torch::kFloat32).clone();

                // Truncated BPTT over windows of cfg_.bptt steps. Hidden is
                // carried across windows but detached at each boundary.
                auto h = torch::zeros({B, H},
                                      torch::TensorOptions().dtype(torch::kFloat32).device(device));
                net->train();
                for (int w0 = 0; w0 < maxT; w0 += cfg_.bptt) {
                    int w1 = std::min(maxT, w0 + cfg_.bptt);

                    auto win_obs  = t_obs .slice(0, w0, w1).to(device);
                    auto win_cont = t_cont.slice(0, w0, w1).to(device);
                    auto win_disc = t_disc.slice(0, w0, w1).to(device);
                    auto win_mask = t_mask.slice(0, w0, w1).to(device);

                    opt.zero_grad();
                    torch::Tensor cont_loss_sum = torch::zeros({}, torch::TensorOptions().device(device));
                    torch::Tensor disc_loss_sum = torch::zeros({}, torch::TensorOptions().device(device));
                    torch::Tensor valid_sum     = torch::zeros({}, torch::TensorOptions().device(device));

                    for (int t = 0; t < (w1 - w0); ++t) {
                        auto obs_t  = win_obs[t];                 // [B,O]
                        auto cont_t = win_cont[t];                // [B,C]
                        auto disc_t = win_disc[t];                // [B]
                        auto mask_t = win_mask[t];                // [B]

                        auto [cm, cs, dl, v, h_out] = net->forward_step(obs_t, h);
                        h = h_out;

                        auto cont_l = ((cm - cont_t).pow(2)).sum(1);          // [B]
                        auto logp   = torch::log_softmax(dl, 1);              // [B,CATS]
                        auto ce     = -logp.gather(1, disc_t.unsqueeze(1)).squeeze(1);  // [B]
                        auto wsamp  = class_w.index_select(0, disc_t);        // [B]

                        cont_loss_sum = cont_loss_sum + (cont_l * mask_t).sum();
                        disc_loss_sum = disc_loss_sum + (ce * wsamp * mask_t).sum();
                        valid_sum     = valid_sum + mask_t.sum();
                    }

                    auto denom = valid_sum.clamp_min(1.0f);
                    auto loss  = (cont_loss_sum + disc_loss_sum) / denom;
                    loss.backward();
                    torch::nn::utils::clip_grad_norm_(net->parameters(), 0.5);
                    opt.step();

                    h = h.detach();

                    float vcount = valid_sum.item<float>();
                    epoch_cont  += cont_loss_sum.item<float>();
                    epoch_disc  += disc_loss_sum.item<float>();
                    epoch_steps += static_cast<long>(vcount);
                }
                ++n_groups;
            }

            double mc = epoch_steps > 0 ? epoch_cont / epoch_steps : 0.0;
            double md = epoch_steps > 0 ? epoch_disc / epoch_steps : 0.0;
            spdlog::info("BC epoch {}/{}: groups={} steps={} "
                         "cont_mse={:.4f} disc_ce={:.4f} total={:.4f}",
                         ep + 1, cfg_.epochs, n_groups, epoch_steps,
                         mc, md, mc + md);
        }

        // Save module weights only (sibling .opt deliberately omitted so PPO
        // --load starts finetuning with a fresh Adam state).
        net->to(torch::kCPU);
        torch::save(net, cfg_.out_path);
        spdlog::info("BC: saved cloned policy to {}", cfg_.out_path);
        return true;
    }

private:
    struct Episode { int start; int len; };

    bool load_demos(int expect_obs, int expect_cont) {
        for (const auto& path : cfg_.demo_paths) {
            std::FILE* f = std::fopen(path.c_str(), "rb");
            if (!f) {
                spdlog::error("BC: cannot open demo file '{}'", path);
                return false;
            }
            char magic[4];
            uint32_t version = 0, obs_dim = 0, cont_dim = 0;
            uint64_t n = 0;
            if (std::fread(magic, 1, 4, f) != 4 ||
                std::memcmp(magic, "DPB1", 4) != 0) {
                spdlog::error("BC: bad magic in '{}'", path);
                std::fclose(f); return false;
            }
            std::fread(&version,  sizeof(uint32_t), 1, f);
            std::fread(&obs_dim,  sizeof(uint32_t), 1, f);
            std::fread(&cont_dim, sizeof(uint32_t), 1, f);
            std::fread(&n,        sizeof(uint64_t), 1, f);
            if (static_cast<int>(obs_dim) != expect_obs ||
                static_cast<int>(cont_dim) != expect_cont) {
                spdlog::error("BC: dim mismatch in '{}' (obs {} vs {}, cont {} vs {})",
                              path, obs_dim, expect_obs, cont_dim, expect_cont);
                std::fclose(f); return false;
            }

            int ep_start = static_cast<int>(n_rec_);
            for (uint64_t i = 0; i < n; ++i) {
                size_t base = obs_.size();
                obs_.resize(base + obs_dim);
                if (std::fread(obs_.data() + base, sizeof(float), obs_dim, f) != obs_dim) break;
                size_t cbase = cont_.size();
                cont_.resize(cbase + cont_dim);
                if (std::fread(cont_.data() + cbase, sizeof(float), cont_dim, f) != cont_dim) break;
                uint32_t didx = 0, done = 0;
                std::fread(&didx, sizeof(uint32_t), 1, f);
                std::fread(&done, sizeof(uint32_t), 1, f);
                disc_.push_back(static_cast<int64_t>(didx));
                ++n_rec_;
                if (done) {
                    int len = static_cast<int>(n_rec_) - ep_start;
                    if (len >= cfg_.min_eplen) {
                        if (cfg_.max_eplen > 0 && len > cfg_.max_eplen)
                            len = cfg_.max_eplen;
                        episodes_.push_back({ep_start, len});
                        int blen = std::min(cfg_.build_len, len);
                        for (int r = 0; r < cfg_.build_oversample; ++r)
                            episodes_.push_back({ep_start, blen});
                    } else {
                        ++dropped_eps_;
                    }
                    ep_start = static_cast<int>(n_rec_);
                }
            }
            // Trailing partial episode (file ended without a done flag).
            int tail = static_cast<int>(n_rec_) - ep_start;
            if (tail > 0) {
                if (tail >= cfg_.min_eplen) {
                    if (cfg_.max_eplen > 0 && tail > cfg_.max_eplen)
                        tail = cfg_.max_eplen;
                    episodes_.push_back({ep_start, tail});
                    int blen = std::min(cfg_.build_len, tail);
                    for (int r = 0; r < cfg_.build_oversample; ++r)
                        episodes_.push_back({ep_start, blen});
                } else {
                    ++dropped_eps_;
                }
            }
            std::fclose(f);
            spdlog::info("BC: '{}' -> {} records", path, n);
        }
        return true;
    }

    BCConfig cfg_;
    std::vector<float>   obs_;
    std::vector<float>   cont_;
    std::vector<int64_t> disc_;
    std::vector<Episode> episodes_;
    uint64_t             n_rec_ = 0;
    int                  dropped_eps_ = 0;
};

}  // namespace dp::policy
