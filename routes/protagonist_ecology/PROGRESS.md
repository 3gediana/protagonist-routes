
[2026-06-01] protagonist_ecology candidate_1 (sharpen sensory encoding) -- VERDICT NEGATIVE (does not break PLATEAU)
  +5-dim ProtagonistRhythmPerception: cyclic day sin/cos, cyclic year sin/cos, d(core_temp)/dt. flag evolution.protagonist_rhythm_perception_enabled (default off). new exe neural-eco-protagonist-rhythm (production exe untouched).
  Gates: (a) C7 no-extinction PASS; (c) no-crash + harmony chain PASS; (b) determinism = PRE-EXISTING mwh/triple_world non-det (gen0/1 bit-identical, diverges gen2; OFF baseline same; dense_gpu=false + batched=false in logs; git: GPU/batched levers rejected). Judged determinism-neutral vs baseline, NOT my change.
  Long 80gen x 3seed x ON/OFF, gen79 fire-vs-coldtrough harmonic: ON R2~0.04-0.07 lead~0-2s in-phase amp~0.005; OFF R2~0.09-0.22 lead +3..+13s (2/3 LEAD). ON not stronger than OFF -> negative.
  Recommend decision-maker: move to candidate_2 (warmth/fire GOAL channel). Local commit only (protagonist_ecology: prefix), NOT pushed. deep_protagonist untouched. See inbox/round_10.

[2026-06-01] protagonist_ecology candidate_2 (homeostatic warmth-drive goal channel) -- VERDICT NEGATIVE -> HARD STOP-LOSS TRIGGERED
  +2-dim ProtagonistWarmthDrivePerception (internal GOAL/DRIVE, NOT reward): warmth_need = clamp((setpoint37 - predicted_core_temp)/span, 0,1) with predicted = cur + horizon*d(T)/dt (horizon=5% day), plus trend d(core_temp)/dt. flag evolution.protagonist_warmth_drive_enabled (default off). Does NOT stack candidate_1 (rhythm off). same -rhythm exe; production exe untouched.
  Gates: (a) C7 no-extinction PASS (smoke gen11 8 species + protagonist positive, 25/48 survived); (c) no-crash + harmony PASS (worksite/fire/chop/craft/hunt/signal all fire); (b) determinism = same pre-existing mwh non-det, determinism-neutral.
  Long 80gen x 3seed ON(drive) vs OFF(candidate_1 OFF aggs), gen79 fire-vs-coldtrough harmonic: ON R2 mean 0.060 (0.06/0.07/0.05), 0/3 LEAD (2 in-phase, 1 no-rhythm); OFF R2 mean 0.133 (0.09/0.09/0.22), 2/3 LEAD. ON amp marginally higher on 2 seeds but lower R2 + no phase lead. ON NOT > OFF -> HARD GATE FAILED.
  Per decision_round_10 stop-loss: no ON>OFF harmonic across two bloodlines (c1 inputs, c2 drive) -> env selection pressure judged insufficient -> pivot to ENV pressure change (harsher night cold / narrower night survival window) OR accept plateau. This is blueprint-direction change -> pinged user + awaiting decision_round_11. Local commit only (protagonist_ecology:), NOT pushed. deep_protagonist untouched. See inbox/round_11.

## [round_12 | 2026-06-01 UTC] ENV pivot: harsh night cold (cold_temp_threshold 22->25), single knob, USER-APPROVED option 1
- smoke C7 PASS (8 species +, protagonist 20-40/48, harmony 7/7); knob bites (protag avg +870->~0 tax, no collapse).
- long 80gen x 3seed x ON/OFF (cold25): all reached gen80, C7 intact at gen79 (all species +, protag avg +3005..4477 adapted).
- gen79 fire-anticipation: HARSH OFF 3/3 LEAD (+9/+8/+12s, R2 0.073); HARSH ON(warmth-drive) 0/3 LEAD (R2 0.067); MILD OFF ref 2/3 LEAD (R2 0.133).
- VERDICT A: warmth-drive (candidate_2) NEGATIVE in BOTH envs (ON never > OFF) -> conclusively drop.
- VERDICT B: env pressure PARTIAL POSITIVE on baseline: anticipation became UNIVERSAL (2/3->3/3 LEAD, mean lead +7.3->+9.7s) BUT R2/amplitude still at plateau ceiling (no strength break).
- next (decision-maker): push env knob harder (cold 25->27/28 OR add thermal-damage knob), one at a time, smoke-first, C7 head gate; drop warmth-drive.
- commit local (protagonist_ecology:), not pushed; production exe + deep_protagonist untouched; traces pruned, D free >=200GB.
