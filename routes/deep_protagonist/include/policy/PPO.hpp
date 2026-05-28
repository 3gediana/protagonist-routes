#pragma once

#include "agent/AgentAction.hpp"
#include "agent/Observation.hpp"

#include <torch/torch.h>

#include <cstdint>
#include <vector>

namespace dp::policy {

// =============================================================================
// Network. obs -> MLP encoder -> GRU recurrent core -> three heads:
//   * cont mean      (3) for continuous actions (move_x, move_y, yaw_rate)
//   * disc logits    (17) for discrete categorical action: index in
//                    [0..15] picks one trigger from AgentAction; index
//                    16 = NOOP (don't trigger anything this tick).
//                    Multinomial / softmax replaces the older 16x
//                    independent Bernoulli heads, which suffered from a
//                    "every button half-pressed" collapse (see D-039).
//   * value          (1) state-value baseline for advantage estimation
//
// log_std is a state-independent learnable parameter vector (3), the
// usual PPO trick for continuous control.
//
// The GRU gives the policy short-term memory (~3-10s of effective recall
// at our 30 Hz tick). Long-term spatial recall is supplied externally by
// SpatialMemory (read out into obs).
//
// Recurrent state shape: [B, HIDDEN]. Persists across PPO steps within
// an episode; reset to zero at episode boundary.
// =============================================================================
struct ActorCriticImpl : torch::nn::Module {
    static constexpr int OBS_DIM   = dp::agent::ObservationBuilder::SIZE;
    static constexpr int CONT_DIM  = dp::agent::ACTION_CONTINUOUS_DIM;
    static constexpr int DISC_DIM  = dp::agent::ACTION_DISCRETE_DIM;   // 16 trigger ids
    static constexpr int DISC_CATS = DISC_DIM + 1;                     // 17 = 16 + NOOP
    static constexpr int NOOP_IDX  = DISC_DIM;                         // 16
    static constexpr int HIDDEN    = 256;

    torch::nn::Linear   encoder{nullptr};
    torch::nn::GRUCell  gru{nullptr};
    torch::nn::Linear   cont_mean{nullptr};
    torch::nn::Linear   disc_logits{nullptr};
    torch::nn::Linear   value{nullptr};
    torch::Tensor       log_std;   // shape [CONT_DIM]

    ActorCriticImpl();

    // Single-step forward. obs:[B, OBS_DIM], h_in:[B, HIDDEN].
    // Returns (cont_mean[B,C], cont_log_std[B,C], disc_logits[B,D],
    //          value[B], h_out[B,HIDDEN]).
    std::tuple<torch::Tensor, torch::Tensor, torch::Tensor,
               torch::Tensor, torch::Tensor>
    forward_step(const torch::Tensor& obs, const torch::Tensor& h_in);
};
TORCH_MODULE(ActorCritic);

// =============================================================================
// Per-tick storage for the rollout. Keeps tensors on host until update.
// =============================================================================
struct Transition {
    std::vector<float>  obs;        // OBS_DIM floats
    std::vector<float>  cont_act;   // CONT_DIM floats
    int                 disc_act = ActorCriticImpl::NOOP_IDX;  // category index in [0..DISC_CATS)
    std::vector<float>  h_in;       // HIDDEN floats - GRU hidden BEFORE this step
    float               reward = 0.0f;
    float               value  = 0.0f;
    float               log_prob = 0.0f;
    bool                done   = false;
};

// =============================================================================
// PPO trainer. Plug an environment in via the obs/step lambda interface so
// we don't depend on dp::env::Environment at this header level (avoids
// pulling raylib through the chain).
// =============================================================================
struct PPOConfig {
    int    rollout_steps   = 2048;
    int    epochs          = 8;   // D-077: doubled from 4. Push GPU util 38% -> 60-70%. PPO robust to 4-30 epochs range.
    int    minibatch_size  = 512; // D-077: doubled from 256. Reduce minibatch overhead, raise GPU saturation per step.
    float  gamma           = 0.99f;
    float  lambda_         = 0.95f;
    float  clip_eps        = 0.2f;
    float  vf_coef         = 0.5f;
    float  ent_coef        = 0.01f;   // D-073: D-071 (ent=0.01, fresh init) reached score 0.158 > D-072 (ent=0.02, fresh init) score 0.114. Lower ent_coef wins for fresh init. Use 0.01 going forward.
    float  lr              = 3e-4f;   // D-072: revert to 3e-4 (D-065 baseline). D-070 1e-4 had same endpoint as 3e-4 (drift hypothesis falsified). For fresh init exploration use D-065's original 3e-4.
    float  max_grad_norm   = 0.5f;
    bool   use_cuda        = true;
    int    seed            = 1234;
};

class PPO {
public:
    explicit PPO(const PPOConfig& cfg);

    // Reset the policy's persistent hidden state to zeros (call at the
    // start of every episode).
    void reset_hidden();

    // Pick an action for one observation. Updates the policy's internal
    // hidden state from h_in to h_out. The caller must:
    //   * record h_in (via current_hidden() *before* this call) into the
    //     Transition for replay during update.
    //   * call reset_hidden() at every episode boundary.
    dp::agent::AgentAction sample_action(
        const dp::agent::ObservationBuilder::Vector& obs,
        std::vector<float>&  out_cont,
        int&                 out_disc_idx,
        float& out_log_prob,
        float& out_value);

    // Read the current persistent hidden state (HIDDEN floats). Snapshot
    // this before each sample_action so the Transition records what the
    // GRU saw at that step.
    std::vector<float> current_hidden() const;

    // Compute and apply a PPO update from the given rollout. last_value
    // is V(s_T) for advantage bootstrapping (0 if last step ended episode).
    // Returns mean policy loss / value loss / entropy for logging.
    struct UpdateStats {
        float policy_loss = 0.0f;
        float value_loss  = 0.0f;
        float entropy     = 0.0f;
        float approx_kl   = 0.0f;
        float clip_frac   = 0.0f;
    };
    UpdateStats update(const std::vector<Transition>& rollout,
                       float last_value);

    // Persist / restore. Saves both the network weights AND the optimizer
    // state to a single file using libtorch's archive format. Use this
    // between training sessions so the next run picks up exactly where
    // we stopped (counts, momentum, log_std all included).
    void save(const std::string& path) const;
    void load(const std::string& path);

    // Convenience for logging.
    torch::Device device() const { return device_; }

private:
    PPOConfig                                cfg_;
    torch::Device                            device_;
    ActorCritic                              net_;
    std::shared_ptr<torch::optim::Adam>      opt_;
    // Persistent recurrent state. Shape [1, HIDDEN] on `device_`.
    torch::Tensor                            hidden_;
};

}  // namespace dp::policy
