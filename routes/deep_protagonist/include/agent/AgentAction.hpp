#pragma once

#include <cstdint>

namespace dp::agent {

// One tick worth of "what does the agent want to do".
//
// This is the single contract between any controller (hand-coded wander,
// keyboard input, or PPO policy) and the rest of the simulation.
// Keeping it tight makes it cheap to feed PPO outputs straight into the
// consumer loop in S4.
//
// Continuous channels are clamped by CapsuleAgent::apply_action; callers do
// not need to clip themselves. Discrete triggers are read out by the
// outer loop (or Environment) via agent.last_action().
//
// IMPORTANT: every key in main.cpp must end up as a flag here.
// If you add a new keyboard binding, you MUST add a matching bool below
// AND consume agent.last_action().<bool> in the loop. PPO can only fire
// flags that exist in this struct.
struct AgentAction {
    // ---- Continuous channels (3) ------------------------------------------
    // Desired horizontal move direction in world XY plane. (move_x, move_y)
    // is treated as a 2D vector: its length encodes "how hard to push" in
    // [0, 1] (anything > 1 gets normalized down). 0 = stand still.
    float move_x = 0.0f;
    float move_y = 0.0f;

    // Yaw rotation rate in rad/s. Positive = counter-clockwise (looking
    // down from +Z). Used for steering / camera headings.
    float yaw_rate = 0.0f;

    // ---- Basic triggers (7, S3.7) -----------------------------------------
    // True = "try this action this tick"; false = ignore. Multiple flags
    // can be true at once (e.g. eat + collect on the same tick), but only
    // one resolves per system.
    bool eat            = false;  // E: harvest nearby ripe plants/farm
    bool drink          = false;  // Q: drink from river/swamp + catch fish
    bool attack         = false;  // F: melee attack closest hostile in arc
    bool collect        = false;  // R fallback: pick up resource
    bool place_shelter  = false;  // B: anchor lean-to shelter (1W+1G)
    bool craft_spear    = false;  // C: craft 1 Wood + 1 Stone -> Spear
    bool sleep          = false;  // (reserved) rest at shelter

    // ---- S3.10 expansion (9) ---------------------------------------------
    // Crafting & wardrobe (early/mid)
    bool craft_grass_dress = false;  // X: 3 Grass -> GrassDress
    bool craft_fur_cloak   = false;  // Z: 2 Fur   -> FurCloak
    bool wear_clothes      = false;  // V: put on best available

    // Real construction (mid)
    bool place_blueprint     = false;  // P: place ConstructionSite at feet
    bool cycle_building_type = false;  // Tab: rotate Shed->Wood->Stone->Big
    bool deposit_to_site     = false;  // R primary: drop matching mats in site

    // Farming & taming (late)
    bool plant_seed = false;  // N: plant seed at feet (Late only)
    bool water_plot = false;  // J: water nearest plot (Late only)
    bool feed_tame  = false;  // K: feed nearest animal to build trust (Late only)

    // ---- D-112 Stage3 Level-2 fire (1) -----------------------------------
    // Campfire: build a new fire at feet OR refuel the nearest one. Costs
    // 1 Wood; a lit fire warms the agent within radius and burns down over
    // time (see world/FireSystem). Mapped to categorical index 17 (AFTER
    // NOOP=16) so existing recorded demos keep their meaning.
    bool tend_fire  = false;  // G: build/refuel a campfire at feet

    // ---- D-122 one-pot tech-tree triggers (reserved, append-only) --------
    // Locked in NOW so the discrete categorical NEVER changes size again
    // (training iron law: "网络输出包含所有动作，即使早期没用"). Each maps to a
    // NEW categorical class AFTER tend_fire (ids 18..22), so all existing ids
    // 0..17 — and every recorded demo — keep their meaning. Until its
    // curriculum stage unlocks it, the class is masked to -inf in
    // PPO::apply_disc_mask AND its logit row is zero-init by warm-start
    // surgery, so a warm-started champion behaves byte-identically.
    bool cook           = false;  // cook raw meat at a lit fire -> protein-rich cooked meat (idx 18)
    bool mine           = false;  // mine an ore node while sheltering in a cave; costs stamina (idx 19)
    bool craft_axe      = false;  // craft stone axe -> faster chopping                       (idx 20)
    bool craft_pickaxe  = false;  // craft stone pickaxe -> faster mining                      (idx 21)
    bool build_monument = false;  // haul/advance the capstone monument (great furnace)        (idx 22)
};

// Total channel count for sizing PPO action heads.
//   3 continuous + 16 legacy discrete triggers (ids 0..15) + NOOP (16).
// D-112: tend_fire is appended as categorical id 17, AFTER NOOP, so it is
// NOT part of ACTION_DISCRETE_DIM (which stays 16 to keep NOOP_IDX==16 and
// all previously recorded demo indices valid). The categorical class count
// (DISC_CATS) is widened to 18 in policy/PPO.hpp.
// D-122 one-pot: 5 reserved tech-tree triggers appended as categorical ids
// 18..22 (cook/mine/craft_axe/craft_pickaxe/build_monument). ACTION_DISCRETE_DIM
// STILL stays 16 (NOOP_IDX==16 fixed); DISC_CATS widens to 23 in policy/PPO.hpp.
constexpr int ACTION_CONTINUOUS_DIM = 3;
constexpr int ACTION_DISCRETE_DIM   = 16;

// Convenience: a no-op action (the default) means "stand still, do nothing".
inline AgentAction noop_action() { return AgentAction{}; }

}  // namespace dp::agent
