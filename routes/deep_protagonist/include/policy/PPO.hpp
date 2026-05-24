#pragma once

#include "agent/AgentAction.hpp"
#include "agent/Observation.hpp"

#include <torch/torch.h>

#include <cstdint>
#include <vector>

namespace dp::policy {

// =============================================================================
// Network. Shared MLP trunk -> three heads:
//   * cont mean      (3) for continuous actions (move_x, move_y, yaw_rate)
//   * disc logits    (16) for discrete-trigger actions (eat, drink, ...)
//   * value          (1) state-value baseline for advantage estimation
// log_std is a state-independent learnable parameter vector (3), the
// usual PPO trick for continuous control.
// =============================================================================
struct ActorCriticImpl : torch::nn::Module {
    static constexpr int OBS_DIM  = dp::agent::ObservationBuilder::SIZE;       // 145
    static constexpr int CONT_DIM = dp::agent::ACTION_CONTINUOUS_DIM;          // 3
    static constexpr int DISC_DIM = dp::agent::ACTION_DISCRETE_DIM;            // 16
    static constexpr int HIDDEN   = 256;

    torch::nn::Linear fc1{nullptr};
    torch::nn::Linear fc2{nullptr};
    torch::nn::Linear cont_mean{nullptr};
    torch::nn::Linear disc_logits{nullptr};
    torch::nn::Linear value{nullptr};
    torch::Tensor     log_std;   // shape [CONT_DIM]

    ActorCriticImpl();

    // Forward returns: (cont_mean, cont_log_std, disc_logits, value).
    // obs: [B, OBS_DIM]. All outputs are [B, *].
    std::tuple<torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor>
    forward(const torch::Tensor& obs);
};
TORCH_MODULE(ActorCritic);

// =============================================================================
// Per-tick storage for the rollout. Keeps tensors on host until update.
// =============================================================================
struct Transition {
    std::vector<float>  obs;        // OBS_DIM floats
    std::vector<float>  cont_act;   // CONT_DIM floats
    std::vector<float>  disc_act;   // DISC_DIM floats (0/1)
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
    int    epochs          = 4;
    int    minibatch_size  = 256;
    float  gamma           = 0.99f;
    float  lambda_         = 0.95f;
    float  clip_eps        = 0.2f;
    float  vf_coef         = 0.5f;
    float  ent_coef        = 0.01f;
    float  lr              = 3e-4f;
    float  max_grad_norm   = 0.5f;
    bool   use_cuda        = true;
    int    seed            = 1234;
};

class PPO {
public:
    explicit PPO(const PPOConfig& cfg);

    // Pick an action for one observation. Returns AgentAction filled in,
    // and via out_log_prob / out_value / out_cont / out_disc returns the
    // raw rollout fields the caller stuffs into a Transition.
    dp::agent::AgentAction sample_action(
        const dp::agent::ObservationBuilder::Vector& obs,
        std::vector<float>&  out_cont,
        std::vector<float>&  out_disc,
        float& out_log_prob,
        float& out_value);

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
};

}  // namespace dp::policy
