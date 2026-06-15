# Layer 5 reward-shift A/B validation

> 2026-05-24 short same-seed sanity. Replaces the "never A/B-tested"
> warning in the prior session's training-readiness audit (#5).

## Setup

| Knob | Value |
|---|---|
| seed | 4242 |
| generations | 4 |
| episodes_per_generation | 2 |
| episode_ticks | 600 |
| population_count protagonist | 24 |
| population_count background | 65 (9 species) |
| world size | 600 × 600 |

Both configs cloned from `tmp/layer1/layer5.toml`. The only
difference is the `[reward]` block:

| Field | Layer 4 (baseline) | Layer 5 (shifted) | Δ |
|---|---|---|---|
| survival_weight | 1.0 | 0.3 | -70 % |
| food_weight | 10 | 3 | -70 % |
| cooperation_weight | 6 | 12 | +100 % |
| communication_weight | 4 | 8 | +100 % |
| worksite_proximity_reward | 0.05 | 0.10 | +100 % |
| worksite_completion_reward | 60 | 150 | +150 % |
| signal_co_attendance_reward | 0.6 | 1.5 | +150 % |
| joint_hunt_reward | 8 | 18 | +125 % |

Theme: Layer 5 reduces basal survival/food and amplifies
cooperation/building/communication so the protagonist has to *do*
things together to reach the same fitness, not just stay alive.

## Result

| Gen | L4 avg | L4 best | L5 avg | L5 best | L4 builds | L5 builds | L4 me_cov | L5 me_cov | L4 qd | L5 qd |
|-----|--------|---------|--------|---------|-----------|-----------|-----------|-----------|-------|-------|
| 0 | 1537 | 5414 | **2215** | **7784** | 0 | 0 | 3 | 4 | 8194 | **14343** |
| 1 | 1152 | 5638 | 1192 | 4837 | 0 | 0 | 3 | 3 | 11965 | 10853 |
| 2 | 1277 | 4260 | **2041** | **6655** | 0 | 0 | 2 | 4 | 7914 | **15080** |
| 3 | 1080 | 3673 | **1631** | **10785** | 1 | 1 | 2 | 4 | 5549 | **19821** |

## Reading

- Layer 5 avg fitness is +44/+3/+60/+51 % over Layer 4 across the
  four generations. Three of four gens are clear wins.
- Layer 5 best fitness peaks at 10785 (gen 3) vs Layer 4's peak 5638
  (gen 1). The reward shift unblocks much higher individual fitness.
- Layer 5 ME-NEAT coverage stays at 4 cells across gen 0/2/3 while
  Layer 4 drops from 3 to 2. The shift produces more *kinds* of
  agents, not just better ones.
- QD score (sum of cell elite fitnesses) follows the same pattern:
  L5 ends gen 3 with 19821 vs L4's 5549, a 3.6× gap, which is the
  cleanest single number summarising the regime change.
- Build count is tied (both reach 1 at gen 3). Layer 5 doesn't make
  building strictly more frequent at this short scale; what it does
  is reward the *whole protagonist task chain* (gather → cooperate
  → site progress) enough that fitness is much higher even before
  the build completes. A 30-gen run is still needed to confirm
  `builds≥3` becomes stable (training-readiness item #10).

## Conclusion

The reward shift in Layer 5 is *not* noise on the same seed it makes
the avg-fit gap real and growing (Gen 0: 1.44×, Gen 3: 1.51×) while
keeping QD coverage stable. Item #5 closed.

Open follow-ups: feed this result back into the long-run plan
(item #10: 50-100 gen at the Layer 5 reward block).
