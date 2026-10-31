#pragma once

// Phase E territory layer (2026-06): a lightweight, decaying "scent-mark" store.
//
// This is a world-state LAYER in the same family as CampfireLayer / WorksiteLayer
// / BaseLayer -- NOT a new map or world. Each mark is a small claimed point that
// fades over time. Agents drop marks via SocialActionCapability's mark output;
// the marks are read back by:
//   * SocialActionCapability (evict) -- you may evict intruders on land you hold,
//   * TerritorySocialPerception      -- "am I on my own land" / foreign pressure.
//
// Ownership is by GENETIC FINGERPRINT (a clan signature), not by agent id, so a
// mark outlives its owner and is "owned" by any close relative
// (relatedness >= 0.6, the same kin threshold used everywhere else).

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Vec2.h"

#include <cstddef>
#include <span>
#include <vector>

namespace neuro {
class IWorld;
}

namespace neuro::routes::protagonist {

struct TerritoryMark {
    Vec2 position;
    std::vector<float> owner_fingerprint;  // copy of the owner's genetic fingerprint
    float strength;                        // 0..1; decays each tick, culled near 0
    float radius;                          // claim radius around `position`
};

class TerritoryMarkLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_territory_mark";

    explicit TerritoryMarkLayer(float mark_radius = 12.0f,
                                float decay_per_second = 0.02f,
                                float merge_radius = 8.0f,
                                float refresh_boost = 0.5f,
                                std::size_t max_marks = 256);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& /*world*/) override {}
    // Decay every mark and cull the ones that have faded to nothing.
    void tick(IWorld& world, float dt) override;
    // After campfire (150); this layer reads no agent state, only ages itself.
    int tickOrder() const override { return 160; }

    // Agent-driven: drop or refresh a mark owned by `owner_fp` at `pos`.
    // If an owned (kin) mark already sits within merge_radius, that mark is
    // refreshed (strength boosted toward 1, position nudged toward `pos`)
    // instead of spawning a duplicate. When the store is full the weakest mark
    // is recycled. Returns true if a mark was created or refreshed.
    bool placeOrRefresh(Vec2 pos, std::span<const float> owner_fp);

    // Strength (0..1) of the strongest mark covering `pos` that is OWNED by
    // `owner_fp` (relatedness >= kin threshold). 0 if none -> "not my land".
    float ownedStrengthAt(Vec2 pos, std::span<const float> owner_fp) const;

    // Strength (0..1) of the strongest mark covering `pos` that is FOREIGN to
    // `owner_fp` (relatedness < kin threshold). 0 if none. Used as a standing
    // territorial-threat signal even when no rival agent is currently nearby.
    float foreignStrengthAt(Vec2 pos, std::span<const float> owner_fp) const;

    bool inOwnedTerritory(Vec2 pos, std::span<const float> owner_fp) const {
        return ownedStrengthAt(pos, owner_fp) > 0.0f;
    }

    const std::vector<TerritoryMark>& marks() const { return marks_; }
    std::size_t markCount() const { return marks_.size(); }

private:
    float mark_radius_;
    float decay_per_second_;
    float merge_radius_;
    float refresh_boost_;
    std::size_t max_marks_;
    std::vector<TerritoryMark> marks_;
};

}  // namespace neuro::routes::protagonist
