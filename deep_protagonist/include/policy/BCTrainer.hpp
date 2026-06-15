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
    int   trigger_oversample    = 0;      // replicate a short window AROUND each non-NOOP
                                          // (trigger) event this many times. Unlike
                                          // build_oversample -- which replays the first
                                          // build_len steps including the long walk-to-site,
                                          // biasing the continuous head toward movement so the
                                          // clone wanders instead of camping -- this amplifies
                                          // ONLY the rare deposit decision in-context, leaving
                                          // the NOOP-camp majority and the near-zero-movement
                                          // continuous targets intact. The oracle deposits ~2x
                                          // early then idles, so this is the clean way to make
                                          // the clone BOTH deposit/build AND camp.
    int   trig_pre              = 8;      // steps before a trigger event to include in its window
    int   trig_post             = 12;     // steps after
    bool  use_cuda              = true;
    int   seed                  = 1234;
    std::vector<std::string> low_os_demos;     // subset of demo_paths using low_trigger_oversample
    int   low_trigger_oversample = -1;         // <0 = unused; else trigger_oversample for low_os_demos (night downweight)
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
        // D-103 throughput: let cuDNN pick the fastest kernels. No effect on
        // results (autotune only), helps keep the GPU saturated.
        if (device.is_cuda()) torch::globalContext().setBenchmarkCuDNN(true);

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

            // D-103: accumulate epoch stats on-device so the per-window
            // GPU->CPU .item() syncs (which stalled the pipeline) are gone;
            // we sync exactly once at the end of the epoch for logging.
            auto dev_opts = torch::TensorOptions().dtype(torch::kFloat32).device(device);
            torch::Tensor epoch_cont_t  = torch::zeros({}, dev_opts);
            torch::Tensor epoch_disc_t  = torch::zeros({}, dev_opts);
            torch::Tensor epoch_steps_t = torch::zeros({}, dev_opts);
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
                //
                // D-103 throughput: move the whole length-padded group to the
                // device ONCE (was: a host->device copy per window), then run
                // the windows entirely on-device. Inside each window the
                // non-recurrent encoder and the output heads are now computed
                // for the ENTIRE window in single batched matmuls instead of
                // W tiny per-timestep ones; only the GRUCell recurrence stays
                // sequential (it inherently must). This is mathematically the
                // same computation and the same per-window optimiser step as
                // before -- identical gradients -- it just keeps the GPU fed
                // (was 35-67% util, 2.9/8G VRAM). No change to the BC
                // objective, the data, or any hyperparameter.
                auto d_obs  = t_obs .to(device);
                auto d_cont = t_cont.to(device);
                auto d_disc = t_disc.to(device);
                auto d_mask = t_mask.to(device);

                auto h = torch::zeros({B, H},
                                      torch::TensorOptions().dtype(torch::kFloat32).device(device));
                net->train();
                for (int w0 = 0; w0 < maxT; w0 += cfg_.bptt) {
                    int w1 = std::min(maxT, w0 + cfg_.bptt);
                    int W  = w1 - w0;

                    auto win_obs  = d_obs .slice(0, w0, w1);   // [W,B,O]
                    auto win_cont = d_cont.slice(0, w0, w1);   // [W,B,C]
                    auto win_disc = d_disc.slice(0, w0, w1);   // [W,B]
                    auto win_mask = d_mask.slice(0, w0, w1);   // [W,B]

                    opt.zero_grad();

                    // Non-recurrent encoder for the whole window at once.
                    auto enc = torch::relu(net->encoder(win_obs));   // [W,B,H]

                    // Recurrent core. D-103b throughput: replace the manual
                    // per-step GRUCell loop (which fired W tiny, launch-bound
                    // kernels per window -- the dominant remaining bottleneck)
                    // with the fused multi-step GRU op run over the whole
                    // window at once, reusing the SAME GRUCell weights. PyTorch
                    // GRU and GRUCell share the identical gate formulation and
                    // (r,z,n) weight ordering, so this is the same recurrence
                    // and the same gradients -- no change to the model, the
                    // saved checkpoint format, the data, or any hyperparameter;
                    // it just lets one fused (cuDNN) kernel do the sequence
                    // instead of W separate launches.
                    auto hx = h.unsqueeze(0);                       // [1,B,H]
                    auto gru_out = torch::gru(
                        enc, hx,
                        {net->gru->weight_ih, net->gru->weight_hh,
                         net->gru->bias_ih,   net->gru->bias_hh},
                        /*has_biases=*/true, /*num_layers=*/1,
                        /*dropout=*/0.0, /*train=*/true,
                        /*bidirectional=*/false, /*batch_first=*/false);
                    auto h_seq = std::get<0>(gru_out);              // [W,B,H]
                    h          = std::get<1>(gru_out).squeeze(0);   // [B,H]

                    // Heads + losses, batched across the window. (value /
                    // log_std heads are intentionally not touched here -- BC
                    // only supervises cont_mean + disc_logits, exactly as the
                    // old per-step loop did.)
                    auto cm   = net->cont_mean(h_seq);              // [W,B,C]
                    auto dl   = net->disc_logits(h_seq);            // [W,B,CATS]
                    auto cont_l = ((cm - win_cont).pow(2)).sum(-1); // [W,B]
                    auto logp   = torch::log_softmax(dl, -1);       // [W,B,CATS]
                    auto ce     = -logp.gather(-1, win_disc.unsqueeze(-1)).squeeze(-1); // [W,B]
                    auto wsamp  = class_w.index_select(0, win_disc.reshape(-1)).reshape({W, B}); // [W,B]

                    auto cont_loss_sum = (cont_l * win_mask).sum();
                    auto disc_loss_sum = (ce * wsamp * win_mask).sum();
                    auto valid_sum     = win_mask.sum();

                    auto denom = valid_sum.clamp_min(1.0f);
                    auto loss  = (cont_loss_sum + disc_loss_sum) / denom;
                    loss.backward();
                    torch::nn::utils::clip_grad_norm_(net->parameters(), 0.5);
                    opt.step();

                    h = h.detach();

                    // Accumulate on-device; no per-window sync.
                    epoch_cont_t  += cont_loss_sum.detach();
                    epoch_disc_t  += disc_loss_sum.detach();
                    epoch_steps_t += valid_sum.detach();
                }
                ++n_groups;
            }

            // D-103: single GPU->CPU sync per epoch (was once per window).
            long   epoch_steps = static_cast<long>(epoch_steps_t.item<float>());
            double epoch_cont  = static_cast<double>(epoch_cont_t.item<float>());
            double epoch_disc  = static_cast<double>(epoch_disc_t.item<float>());
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
            eff_trig_os_ = (cfg_.low_trigger_oversample >= 0 && is_low_os_(path))
                           ? cfg_.low_trigger_oversample : cfg_.trigger_oversample;
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
                        add_trigger_windows(ep_start, ep_start + len);
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
                    add_trigger_windows(ep_start, ep_start + tail);
                } else {
                    ++dropped_eps_;
                }
            }
            std::fclose(f);
            spdlog::info("BC: '{}' -> {} records", path, n);
        }
        return true;
    }

    bool is_low_os_(const std::string& p) const {
        for (const auto& q : cfg_.low_os_demos) if (q == p) return true;
        return false;
    }

    // Scan [ep_start, ep_end) for non-NOOP (trigger) events and replicate a
    // short [t-trig_pre, t+trig_post) window around each, cfg_.trigger_oversample
    // times. The window starts mid-episode (Episode.start is a global index, so
    // this is supported directly); the GRU just starts that window from h=0,
    // the same as any truncated-BPTT boundary.
    void add_trigger_windows(int ep_start, int ep_end) {
        if (eff_trig_os_ <= 0) return;
        for (int g = ep_start; g < ep_end; ++g) {
            if (disc_[g] == ActorCriticImpl::NOOP_IDX) continue;
            int w0 = std::max(ep_start, g - cfg_.trig_pre);
            int w1 = std::min(ep_end,   g + cfg_.trig_post);
            int wlen = w1 - w0;
            if (wlen <= 0) continue;
            for (int r = 0; r < eff_trig_os_; ++r)
                episodes_.push_back({w0, wlen});
        }
    }

    BCConfig cfg_;
    int                  eff_trig_os_ = 0;   // effective trigger_oversample for the file being loaded
    std::vector<float>   obs_;
    std::vector<float>   cont_;
    std::vector<int64_t> disc_;
    std::vector<Episode> episodes_;
    uint64_t             n_rec_ = 0;
    int                  dropped_eps_ = 0;
};

}  // namespace dp::policy
