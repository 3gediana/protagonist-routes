#pragma once

#include "agent/AgentAction.hpp"
#include "agent/Observation.hpp"
#include "agent/RewardSystem.hpp"   // D-123 SF: feature dimension F

#include <torch/torch.h>

#include <array>
#include <cstdint>
#include <utility>
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
    static constexpr int DISC_DIM  = dp::agent::ACTION_DISCRETE_DIM;   // 16 legacy trigger ids (0..15)
    static constexpr int NOOP_IDX  = DISC_DIM;                         // 16 (unchanged; recorded demos store this)
    // D-112 fire (decision_round_10): tend_fire is a NEW categorical class
    // appended AFTER NOOP, so ids 0..16 keep their meaning (incl. old demos'
    // NOOP=16) and previously recorded .bin demos stay valid without remap.
    // NOOP is therefore no longer the last index.
    static constexpr int TEND_FIRE_IDX = NOOP_IDX + 1;                 // 17
    // D-122 one-pot tech-tree: 5 NEW categorical classes appended AFTER
    // tend_fire (append-only, so ids 0..17 + every recorded demo stay valid).
    // Each is zero-init by warm-start surgery AND masked to -inf until its
    // curriculum stage flips the matching PPOConfig.allow_* gate true.
    static constexpr int COOK_IDX           = TEND_FIRE_IDX + 1;       // 18
    static constexpr int MINE_IDX           = COOK_IDX + 1;            // 19
    static constexpr int CRAFT_AXE_IDX      = MINE_IDX + 1;            // 20
    static constexpr int CRAFT_PICKAXE_IDX  = CRAFT_AXE_IDX + 1;       // 21
    static constexpr int BUILD_MONUMENT_IDX = CRAFT_PICKAXE_IDX + 1;   // 22
    static constexpr int DISC_CATS          = BUILD_MONUMENT_IDX + 1;  // 23 = 16 triggers + NOOP + tend_fire + 5 tech-tree
    // S5 FROZEN ARCHITECTURE (locked permanently, append-only obs):
    //   OBS_DIM=582 (573 + 9 D-122 tech-tree reserved, append-only),
    //   HIDDEN=1408, action=3 continuous + 23 categorical (18 + 5 D-122
    //   tech-tree reserved). D-122 is the FINAL one-pot widening: all future
    //   stages (cook/mine/tools/build-tier/capstone) reuse these slots, so the
    //   net never resizes again — every checkpoint transfers forever.
    //   Params = 6*H^2 (GRU) + (OBS+CONT+DISC_CATS+...+1)*H + biases
    //          ~= 12.74M  ->  .pt (weights only) ~= 51MB  ~= user's "50兆".
    // The recurrent GRU core dominates capacity (6*H^2), so width (not a
    // 2048 feed-forward stack) is how we hit the 12.5M/50MB target without a
    // 100MB GRU. From S5 onward EVERY checkpoint shares this shape so they
    // accumulate/transfer forever (zero-pad only for any future obs append).
    // NOTE: D-085's 6.9M expansion regressed on the OLD flat single-biome
    // world; S5 pairs this larger brain with the realistic world (relief +
    // diverse biomes + slope/temp coupling) it actually needs the capacity
    // for, and trades sample-efficiency for ceiling per the owner's decision.
    static constexpr int HIDDEN    = 1408;
    // D-123 Successor-Features: ψ head width = number of reward feature
    // channels φ. Tied to RewardSystem::F so dot(φ, w) stays consistent;
    // a static_assert in main.cpp pins them equal.
    static constexpr int SF_DIM    = dp::agent::RewardSystem::F;   // 12

    torch::nn::Linear   encoder{nullptr};
    torch::nn::GRUCell  gru{nullptr};
    torch::nn::Linear   cont_mean{nullptr};
    torch::nn::Linear   disc_logits{nullptr};
    torch::nn::Linear   value{nullptr};
    // D-123 SF: successor-feature head. ψ(s) predicts the discounted future
    // accumulation of each reward feature φ (length SF_DIM). It is a sibling
    // of `value` reading the SAME hidden h; trained (detached, see psi()) so
    // its gradient never perturbs the champion policy/value/trunk.
    torch::nn::Linear   successor{nullptr};
    torch::Tensor       log_std;   // shape [CONT_DIM]

    ActorCriticImpl();

    // Single-step forward. obs:[B, OBS_DIM], h_in:[B, HIDDEN].
    // Returns (cont_mean[B,C], cont_log_std[B,C], disc_logits[B,D],
    //          value[B], h_out[B,HIDDEN]).
    std::tuple<torch::Tensor, torch::Tensor, torch::Tensor,
               torch::Tensor, torch::Tensor>
    forward_step(const torch::Tensor& obs, const torch::Tensor& h_in);

    // D-123 SF: successor features ψ from a hidden state h:[B,HIDDEN] ->
    // [B,SF_DIM]. Pure linear read-out; does NOT detach internally so callers
    // choose: training detaches h (h.detach()) so only `successor` updates,
    // self-tests may pass a grad-enabled h to check the head itself.
    torch::Tensor psi(const torch::Tensor& h);
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
    // D-123 SF: the UN-weighted reward feature vector φ for this step
    // (RewardSystem::last_features(), length SF_DIM). Empty if the caller
    // never filled it -> ψ TD loss is skipped for that rollout (back-compat).
    std::vector<float>  phi;
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
    float  gamma           = 0.995f;  // D-098: bumped 0.99 -> 0.995. Effective horizon 100->200 ticks (3.3s). D-094..D-097 all hit the same wall: PPO learns shelter early then unlearns it. Root cause is partly the 100-tick horizon - sheltering for a full night (60+s = 3600+ ticks) can't be credit-assigned. Going further (0.999) is unstable on this baseline; 0.995 is a measured step. Pairs with D-098 Environment.cpp regen +3->+15/min so the per-tick value of sheltering rises enough to dominate the local-foraging spike.
    float  lambda_         = 0.95f;
    float  clip_eps        = 0.2f;
    float  vf_coef         = 0.5f;
    float  ent_coef        = 0.01f;   // D-073: D-071 (ent=0.01, fresh init) reached score 0.158 > D-072 (ent=0.02, fresh init) score 0.114. Lower ent_coef wins for fresh init. Use 0.01 going forward.
    float  lr              = 3e-4f;   // D-072: revert to 3e-4 (D-065 baseline). D-070 1e-4 had same endpoint as 3e-4 (drift hypothesis falsified). For fresh init exploration use D-065's original 3e-4.
    float  max_grad_norm   = 0.5f;
    bool   use_cuda        = true;
    int    seed            = 1234;
    int    n_envs          = 1;     // D-078: VecEnv parallel envs in single PPO
    // decision_round_17 (S4 PPO C): warmstart-stability controls.
    bool   mask_atk_fire = false;  // mask discrete idx2(attack)+idx17(fire) to -inf
    // decision_blueprint_nutrition (round_21): unmask attack for hunting while
    // KEEPING fire masked. When true, only idx17(fire) is masked; idx2(attack)
    // is left open so the policy can hunt prey for the protein bar. Mutually
    // exclusive with mask_atk_fire (if both set, atk_fire wins / masks both).
    bool   mask_fire_only = false;
    float  kl_ref_coef   = 0.0f;   // beta for KL(pi||pi_ref=bc_v9) anchor (0=off)
    float  target_kl     = 0.0f;   // CleanRL-style early-stop threshold (0=off)
    // D-122 one-pot tech-tree curriculum gates. Each reserved categorical
    // class (idx 18..22) is masked to -inf UNLESS its gate is true, so a
    // warm-started champion is behaviourally identical to the pre-tech-tree
    // champion (the new logit rows are zero-init AND masked). Flip a gate
    // true to UNLOCK that action for the curriculum stage that teaches it.
    bool   allow_cook       = false;  // idx 18
    bool   allow_mine       = false;  // idx 19
    bool   allow_craft_axe  = false;  // idx 20
    bool   allow_craft_pick = false;  // idx 21
    bool   allow_monument   = false;  // idx 22
    // D-123 Successor-Features: weight of the ψ TD loss added to the PPO
    // objective. ψ is trained on a DETACHED trunk so this term only updates
    // the `successor` head -> zero perturbation to policy/value. 0 disables
    // ψ learning entirely (e.g. legacy runs). 0.5 is the design default.
    float  sf_coef          = 0.5f;
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

    // D-078 VecEnv API. Batched sampling for N parallel envs in one PPO
    // forward call. Reuses the same network and hidden_ tensor (shape
    // [N, HIDDEN]). Per-env hidden snapshots/resets via *_at(idx).
    //
    // out_*: each vector resized to N. cont_acts[i] has CONT_DIM floats.
    void sample_actions_batch(
        const std::vector<dp::agent::ObservationBuilder::Vector>& obs_batch,
        std::vector<dp::agent::AgentAction>& actions_out,
        std::vector<std::vector<float>>& cont_acts_out,
        std::vector<int>& disc_idxs_out,
        std::vector<float>& log_probs_out,
        std::vector<float>& values_out);

    // Snapshot one env's hidden row (HIDDEN floats). Use BEFORE the
    // batched sample call so the Transition records the input state.
    std::vector<float> current_hidden_at(int env_idx) const;

    // Zero a single env's hidden row (call on episode boundary for that env).
    void reset_hidden_at(int env_idx);

    // Compute and apply a PPO update from the given rollout. last_value
    // is V(s_T) for advantage bootstrapping (0 if last step ended episode).
    // Returns mean policy loss / value loss / entropy for logging.
    struct UpdateStats {
        float policy_loss = 0.0f;
        float value_loss  = 0.0f;
        float entropy     = 0.0f;
        float approx_kl   = 0.0f;
        float clip_frac   = 0.0f;
        // D-122 early-warning dashboard. Mean RAW (pre-normalization) GAE
        // advantage of the transitions that chose a watched discrete action,
        // plus how many such transitions there were. The SIGN is a leading
        // indicator of where PPO will push that action's probability next:
        //   >0 => log-prob rises (action becomes more likely)
        //   <0 => log-prob falls (action gets extinguished)
        // Watched: attack(idx 2, the hunt action) and cook(idx 18). Lets you
        // read the trend from a few hundred episodes instead of running 48M
        // steps to wait for the behavior itself to emerge.
        float adv_attack  = 0.0f;
        int   cnt_attack  = 0;
        float adv_cook    = 0.0f;
        int   cnt_cook    = 0;
        // D-123 FULL-behavior panel: same read-only leading-indicator semantics
        // as adv_attack/adv_cook, but for EVERY discrete categorical class
        // (0..DISC_CATS-1) at once. Index = categorical id (see PPO.cpp
        // disc->AgentAction switch). adv_by_action[i] = mean RAW (pre-norm) GAE
        // advantage of transitions that chose action i (sign => that behavior
        // rises/falls next); cnt_by_action[i] = how many such transitions;
        // cnt_by_action[i]/total_transitions = how often the policy is even
        // trying behavior i. A few hundred episodes thus predict which of ALL
        // behaviors will emerge vs get extinguished -- not just hunt/cook.
        // adv_attack==adv_by_action[2], adv_cook==adv_by_action[18] (kept for
        // back-compat with the existing one-line log).
        std::array<float, ActorCriticImpl::DISC_CATS> adv_by_action{};
        std::array<int,   ActorCriticImpl::DISC_CATS> cnt_by_action{};
        int total_transitions = 0;
        // D-123 SF: mean ψ TD loss this update (0 if φ not provided / sf_coef 0).
        float sf_loss = 0.0f;
    };

    // D-122 early-warning helper (pure math, no network / no GPU; unit-tested
    // in main.cpp --self-test). Mean of `adv` over every index i where
    // `disc_act[i] == target`. Returns {mean, count}; count 0 => {0, 0}.
    static std::pair<float, int> mean_adv_for_action(
        const std::vector<float>&   adv,
        const std::vector<int64_t>& disc_act,
        int                         target);
    // D-101: critic_only restricts the loss to the value term. Because the
    // value head is a separate branch (does not feed cont_mean/disc_logits),
    // value_loss.backward() produces ZERO gradient on the policy-head params,
    // so the cloned policy is preserved exactly while the critic (and shared
    // trunk) learns to estimate V for the BC policy. Run this for the first
    // few dozen updates of a BC->PPO finetune: BC trains only the actor, so
    // the critic starts random; without a warmup the first updates compute
    // garbage advantages and destroy the warm-started policy before the
    // critic converges (observed: bldg_ticks 1806 -> 0 by update ~250).
    // D-123 SF: `sf_warmup` trains ONLY the ψ head (policy+value+trunk frozen),
    // analogous to critic_only -- use it for the first few hundred updates after
    // warm-starting a champion that has a randomly-initialised ψ head, so ψ
    // converges before you trust ψ·w' recipe screens. With the detached trunk,
    // the normal (non-warmup) path ALSO leaves the policy untouched by ψ; warmup
    // additionally suppresses the policy/value gradients so nothing moves at all.
    UpdateStats update(const std::vector<Transition>& rollout,
                       float last_value, bool critic_only = false,
                       bool sf_warmup = false);

    // Persist / restore. Saves both the network weights AND the optimizer
    // state to a single file using libtorch's archive format. Use this
    // between training sessions so the next run picks up exactly where
    // we stopped (counts, momentum, log_std all included).
    void save(const std::string& path) const;
    void load(const std::string& path);

    // decision_round_17 (S4 PPO C): load a FROZEN reference policy (bc_v9)
    // for the KL-to-reference anchor. Call AFTER the ctor and any --load
    // warmstart. Enables the beta*KL(pi||pi_ref) penalty in update().
    void load_reference(const std::string& path);

    // Convenience for logging.
    torch::Device device() const { return device_; }

    // D-101: deterministic action mode for eval (mean continuous action,
    // argmax discrete). Used to measure a cloned/finetuned policy without
    // the exploration noise that exp(log_std) and multinomial sampling add.
    void set_deterministic(bool d) { deterministic_ = d; }

private:
    PPOConfig                                cfg_;
    torch::Device                            device_;
    ActorCritic                              net_;
    std::shared_ptr<torch::optim::Adam>      opt_;
    // Persistent recurrent state. Shape [1, HIDDEN] on `device_`.
    torch::Tensor                            hidden_;
    bool                                     deterministic_ = false;
    // decision_round_17 (S4 PPO C):
    ActorCritic                              ref_net_{nullptr}; // frozen KL-ref
    bool                                     has_ref_      = false;
    torch::Tensor                            disc_mask_;        // [1,DISC_CATS]
    bool                                     mask_enabled_ = false;
    bool                                     mask_attack_  = true;  // false => fire-only (hunting world)
    // D-122 one-pot: reserved tech-tree classes (idx 18..22) whose curriculum
    // gate is OFF. Populated once in the ctor from PPOConfig.allow_*; these
    // columns are masked to -inf EVERY step until their stage unlocks them.
    std::vector<int64_t>                     locked_cats_;
    torch::Tensor apply_disc_mask(const torch::Tensor& dl) const {
        if (!mask_enabled_ && locked_cats_.empty()) return dl;
        // In-place writes (index_put_/fill_) and from_blob tensors abort in
        // this libtorch+CUDA build; use ONLY functional ops + a same-shape
        // final add (the proven-safe path). Masked columns get -50 added ->
        // prob ~2e-22 (multinomial-safe) / hard under argmax. fire(17) is
        // always masked when enabled; attack(2) only when mask_attack_ (the
        // nutrition world unmasks attack so the agent can hunt). The reserved
        // tech-tree classes (locked_cats_) are masked until their stage opens.
        auto col  = torch::arange(dl.size(1), dl.options()).unsqueeze(0); // [1,C]
        auto cond = (col == -1.0);                                        // all-false [1,C]
        if (mask_enabled_) {
            cond = cond | (col == (double)ActorCriticImpl::TEND_FIRE_IDX);
            if (mask_attack_) cond = cond | (col == 2);
        }
        for (int64_t k : locked_cats_) cond = cond | (col == (double)k);
        cond      = cond.expand({dl.size(0), dl.size(1)});                // [B,C]
        auto m    = torch::where(cond, torch::full_like(dl, -50.0f),
                                       torch::zeros_like(dl));            // [B,C]
        return dl + m;                                                    // same-shape add
    }
};

}  // namespace dp::policy
