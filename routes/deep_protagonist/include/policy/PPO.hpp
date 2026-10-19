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
    // D1 (discrete-head decoupling): width of the dedicated discrete trunk used
    // only when decouple_disc is ON (see forward_step). Append-only new module,
    // so old checkpoints warm-start via the tolerant loader.
    static constexpr int DISC_HID  = 512;
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
    // D1 discrete-head decoupling. When decouple_disc is ON, the discrete policy
    // reads (a) the shared trunk hidden as a STOP-GRADIENT feature (so the
    // discrete gradient never reshapes the movement-dominated encoder+GRU, the
    // diagnosed root cause of discrete-head collapse) plus (b) its OWN dedicated
    // feed-forward trunk on raw obs. disc_head2 is zero-init so the net is
    // byte-identical to the shared-trunk policy the moment decoupling is enabled;
    // the dedicated path grows from zero (cf. the D-122 tech-tree rows). OFF
    // (default) keeps the legacy disc_logits(h) path exactly.
    torch::nn::Linear   disc_trunk{nullptr};   // OBS_DIM  -> DISC_HID
    // D1.5: dedicated discrete trunk whose input is cat(obs, h.detach()) =
    // OBS_DIM+HIDDEN -> DISC_HID. Used only when disc_mem_ctx is ON. Registered
    // ALWAYS so it serializes / warm-starts tolerantly like disc_trunk.
    torch::nn::Linear   disc_trunk_mem{nullptr}; // OBS_DIM+HIDDEN -> DISC_HID
    torch::nn::Linear   disc_head2{nullptr};   // DISC_HID -> DISC_CATS (zero-init)
    bool                decouple_disc = false; // set from PPOConfig in PPO ctor
    bool                disc_mem_ctx  = false; // set from PPOConfig in PPO ctor
    // D2 (critic balancing): second value head for the SPARSE tech reward
    // stream. Registered ALWAYS (serialize + tolerant warm-start like
    // disc_trunk); ZERO-init so, the moment stream_split is enabled on an old
    // checkpoint, V_tech(s)=0 (the correct prior: tech events are rare) and the
    // survival value path is untouched. Only READ when stream_split is ON.
    torch::nn::Linear   value_tech{nullptr};   // HIDDEN -> 1 (zero-init)
    // D9 CTM-lite (Sakana Continuous Thought Machine, simplified): an internal
    // "thinking" loop that runs ctm_ticks extra ticks AFTER the recurrent core,
    // letting deliberation unfold over an internal axis before the heads read
    // out. Modules registered ALWAYS (tolerant warm-start); ctm_out is
    // ZERO-init so h_out is byte-identical whether or not ticks ran, and the
    // branch only READS when ctm_ticks > 0. Simplifications vs the paper
    // (documented in RESEARCH_FRONTIER 2026-10 §3.1): per-tick history is
    // aggregated by a mean over tick outputs (a sync-feature proxy) instead of
    // the full pairwise correlation matrix, and the NLM is one shared linear
    // (not per-neuron weights).
    static constexpr int CTM_DIM = 128;
    torch::nn::Linear   ctm_in{nullptr};    // HIDDEN -> CTM_DIM  (tick state seed)
    torch::nn::Linear   ctm_nlm{nullptr};   // HIDDEN+CTM_DIM -> CTM_DIM (one shared NLM step)
    torch::nn::Linear   ctm_out{nullptr};   // 2*CTM_DIM -> HIDDEN (zero-init read-out added to h)
    // D9 CfC (Closed-form Continuous-time neuron, Hasani/Lechner line): a
    // drop-in GRU alternative whose units carry learned time constants, so
    // fast survival reactions and slow tech deliberation live on different
    // dynamics instead of fighting for one recurrence (the D8 "skill budget
    // competition" hypothesis). Registered ALWAYS; used INSTEAD of gru only
    // when the use_cfc flag is set. Math: gates from cat(enc, z),
    //   a = softplus(Wa*x+ba), b = tanh(Wb*x+bb), g = sigmoid(Wg*x+bg),
    //   z' = g * (z*exp(-a*dt) + (1-exp(-a*dt))*b),  dt = 1.
    torch::nn::Linear   cfc_Wa{nullptr};    // (HIDDEN+HIDDEN) -> HIDDEN
    torch::nn::Linear   cfc_Wb{nullptr};
    torch::nn::Linear   cfc_Wg{nullptr};
    bool                use_cfc = false;    // set from PPOConfig in PPO ctor
    int                 ctm_ticks = 0;      // set from PPOConfig in PPO ctor
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
    // D2 critic balancing: the part of this step's shaped reward that belongs
    // to the SPARSE tech stream (hunt-chain + repertoire PBRS pulses added by
    // the trainer). The env φ channels carry the rest of the split; the
    // residual (reward - tech stream) stays in the survival stream, so
    // surv+tech == reward exactly regardless of clipping or knob mix. 0 when
    // the caller never filled it -> legacy single-stream behaviour.
    float               reward_tech_extra = 0.0f;
    // D7 experience injection: true when this step's discrete action was
    // RANDOMLY injected (see PPOConfig.inject_disc_prob). Injected steps keep
    // their reward flow (GAE/value) but are EXCLUDED from the PPO policy
    // minibatches, so junk exploration never pollutes the policy gradient.
    // Forced steps (teleport-force) are NOT flagged injected: their stored
    // log_prob is re-priced at the forced category so the update learns from
    // the forced outcome like from a demonstrated action.
    bool                injected = false;
};

// D7: apply a discrete category to an AgentAction. Resets EVERY trigger
// first, so applying onto an already-filled action cleanly replaces the
// sampled trigger (used by the forced-action path in main_train).
void set_disc_trigger(dp::agent::AgentAction& a, int idx);


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
    // Dead-action floor: extra loss term that penalizes discrete actions whose
    // batch-mean probability falls below `dead_action_floor`. Every from-scratch
    // PPO run so far collapsed to ~8-10/23 live actions and the dead logits
    // never recover from reward alone (mechanism, not tuning). The floor keeps
    // a minimal exploration probability alive on EVERY action head so rare
    // milestone actions (craft/build/tame) stay reachable. 0 disables.
    float  dead_action_floor = 0.005f;  // min mean prob per discrete action
    float  dead_action_coef  = 1.0f;    // weight of the revival penalty
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
    // survive-first action mask (diagnostic A/B, default OFF = control). When
    // ON, every discrete action except eat(0)/drink(1) is masked to -50 on any
    // step where the agent is in vital danger (hunger/100 OR thirst/100 below
    // survive_first_thresh). Movement is the continuous head and stays
    // unmasked, so the agent can still navigate to food/water. OFF is
    // byte-identical to the pre-mask config.
    bool   survive_first_mask   = false;
    float  survive_first_thresh = 0.35f;
    // D3: affordance/precondition action mask. Mask a discrete action when the
    // resource its env-handler requires is provably out of reach (read from
    // obs), so the agent cannot waste the step pressing a button with no
    // resource present and is pushed to navigate there first. Default OFF =
    // byte-identical. See apply_afford_mask for the exact gated set + margins.
    bool   afford_mask          = false;
    // D1: decouple the discrete head from the movement-dominated shared trunk
    // (dedicated obs trunk + stop-gradient shared read-out). Default OFF =
    // byte-identical to the shared-trunk policy. See ActorCriticImpl.
    bool   decouple_disc        = false;
    // D1.5: feed the dedicated discrete trunk cat(obs, h.detach()) instead of
    // obs alone, giving its nonlinear path the shared GRU's recurrent memory
    // (still stop-grad). Only meaningful when decouple_disc is ON.
    bool   disc_mem_ctx         = false;
    // D7 experience injection: with this per-decision probability the sampled
    // discrete action is REPLACED by a uniform draw over the currently allowed
    // (mask-respecting) categories. Injected steps are flagged Transition::
    // injected and excluded from the policy update (rewards still flow).
    // Default 0 = byte-identical.
    float  inject_disc_prob     = 0.0f;
    // D9 architecture screens (fresh-init experiments; both default OFF):
    //   ctm_ticks > 0 runs that many internal "thinking" ticks after the
    //   recurrent core (CTM-lite; see ActorCriticImpl comment);
    //   use_cfc swaps the GRU for a closed-form continuous-time cell with
    //   learned per-unit time constants (CfC; multi-timescale dynamics).
    int    ctm_ticks            = 0;
    bool   use_cfc              = false;
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
    // D2 critic balancing (TOPPO-inspired). Diagnosis: the DENSE survival
    // stream (alive/food/water/vital/collect, paid every tick) dominates the
    // shared value head's targets, so when a SPARSE tech event fires
    // (craft/milestone/shelter-build/hunt PBRS) its advantage is noise ->
    // the discrete head gets no learnable signal -> "rolling dice" and
    // deep tech chains pinned at 0 (D1 actor-side decoupling alone did not
    // move this). Fix, all behind one flag:
    //   * split each step's reward into surv + tech streams (exact: tech =
    //     kills/shelter/craft/milestone φ channels + trainer PBRS pulses;
    //     surv = reward - tech),
    //   * GAE per stream, per-stream advantage standardisation,
    //     adv = z(adv_surv) + tech_adv_weight * z(adv_tech),
    //   * a dedicated zero-init value_tech head trained on PopArt-style
    //     standardised tech return targets (running mean/var + output
    //     rescale on stat change) so the sparse stream's value gradient is
    //     scale-balanced against the survival stream's.
    // OFF (default) = byte-identical single-stream PPO.
    bool   stream_split     = false;
    float  tech_adv_weight  = 1.0f;   // w in adv = z(adv_surv) + w*z(adv_tech)
    float  vf_tech_coef     = 1.0f;   // value-loss weight for the tech head
    // φ->reward weight vector used to rebuild the tech stream from the
    // un-weighted φ channels. Filled by main_train from the env's
    // RewardSystem recipe; PPO falls back to the default Weights recipe when
    // left all-zero (env uses default weights).
    std::array<float, static_cast<int>(dp::agent::RewardSystem::F)> phi_weights{};
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
        // D2 critic-balancing diagnostics (stream_split only; 0 otherwise).
        // adv_tech_raw = mean PRE-normalisation tech-stream advantage over the
        // transitions that carried a nonzero tech reward (sign => PPO will
        // push/pull the behaviour that earned it); cnt_tech = how many such
        // transitions were in the rollout; value_loss_tech = mean tech-head
        // MSE; ret_tech_mean = running mean of raw tech returns (PopArt stat).
        float adv_tech_raw    = 0.0f;
        int   cnt_tech        = 0;
        float value_loss_tech = 0.0f;
        float ret_tech_mean   = 0.0f;
        // D7: how many randomly-injected transitions were excluded from the
        // policy minibatches this update (0 unless inject_disc_prob > 0).
        int   injected_excluded = 0;
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

    // D7: injection flags of the LAST sample_* call (per env, 1 = the
    // discrete action was randomly injected). main_train copies this into
    // Transition::injected so update() can exclude those policy steps.
    const std::vector<char>& last_inject_flags() const { return last_inject_flags_; }
    bool last_inject_flag() const { return last_inject_flag_; }
    // D7: log-prob of EVERY discrete category from the LAST sample_* call
    // ([N][DISC_CATS] batched / [DISC_CATS] single-env). Lets main_train
    // re-price a FORCED action's stored log_prob from the same forward pass
    // so the PPO importance ratio stays coherent (no extra forward needed).
    const std::vector<std::vector<float>>& last_disc_log_probs() const { return last_disc_lp_mat_; }
    const std::vector<float>& last_disc_log_probs_row() const { return last_disc_lp_row_; }

private:
    PPOConfig                                cfg_;
    torch::Device                            device_;
    ActorCritic                              net_;
    std::shared_ptr<torch::optim::Adam>      opt_;
    // Persistent recurrent state. Shape [1, HIDDEN] on `device_`.
    torch::Tensor                            hidden_;
    bool                                     deterministic_ = false;
    // D7 experience injection (see PPOConfig.inject_disc_prob) + per-sample
    // discrete log-prob read-out used to re-price forced actions.
    float                                    inject_disc_prob_ = 0.0f;
    std::vector<char>                        last_inject_flags_;
    bool                                     last_inject_flag_ = false;
    std::vector<std::vector<float>>          last_disc_lp_mat_;
    std::vector<float>                       last_disc_lp_row_;
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
    // survive-first mask (see PPOConfig); cached from cfg_ in the ctor.
    bool                                     survive_first_mask_   = false;
    float                                    survive_first_thresh_ = 0.35f;
    bool                                     afford_mask_          = false;
    // D2 critic balancing: cached config + PopArt running stats for the tech
    // return stream. value_tech predicts STANDARDISED tech returns
    // (ret - mean) / std; GAE de-normalises its outputs back to raw return
    // space with these SAME stats so rewards and bootstrap values stay in one
    // scale. On a stat update the head's output layer is rescaled in place
    // (PopArt's precise perturbation) so its raw predictions are unchanged.
    bool                                     stream_split_    = false;
    float                                    tech_adv_weight_ = 1.0f;
    float                                    vf_tech_coef_    = 1.0f;
    std::array<float, static_cast<int>(dp::agent::RewardSystem::F)> phi_w_{};
    float                                    rett_mean_ = 0.0f;
    float                                    rett_std_  = 1.0f;
    bool                                     rett_init_ = false;
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

    // survive-first action mask: on any row whose hunger or thirst (obs cols
    // 0/1, normalised 0..1) is below survive_first_thresh_, every discrete
    // action except eat(0)/drink(1) gets -50 added to its logit (prob ~2e-22,
    // multinomial-safe); vitals safe => full action set. Functional ops + a
    // same-shape add only (CUDA-safe, identical idiom to apply_disc_mask).
    // Applied AFTER apply_disc_mask at every call site (sample + PPO update +
    // KL-ref) so the importance ratio stays consistent. OFF => returns dl.
    torch::Tensor apply_survive_first_mask(const torch::Tensor& dl,
                                           const torch::Tensor& obs) const {
        if (!survive_first_mask_) return dl;
        auto hunger = obs.narrow(1, 0, 1);                                  // [B,1]
        auto thirst = obs.narrow(1, 1, 1);                                  // [B,1]
        auto danger = (hunger < survive_first_thresh_)
                    | (thirst < survive_first_thresh_);                     // [B,1] bool
        auto col    = torch::arange(dl.size(1), dl.options()).unsqueeze(0); // [1,C]
        auto keep   = (col == 0.0) | (col == 1.0);                          // eat,drink
        auto cond   = danger.expand({dl.size(0), dl.size(1)})
                    & (~keep).expand({dl.size(0), dl.size(1)});             // [B,C]
        auto m      = torch::where(cond, torch::full_like(dl, -50.0f),
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
};

}  // namespace dp::policy
