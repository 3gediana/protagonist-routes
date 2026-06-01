# DRAFT smoke plan - candidate_2 stop-loss pivot: harsher night cold (env selection pressure)
STATUS: DRAFT / NOT RUN. Awaiting user direction (option 1 = change env pressure [decision-maker recommended] vs option 2 = accept plateau). Per decision_round_11 §4: executor only drafts config + plan, does NOT run pivot, does NOT change production defaults.

## Rationale (from decision_round_11)
Two bloodlines (candidate_1 richer perception, candidate_2 internal warmth drive) both failed: ON not > OFF on gen79 fire-anticipation. Diagnosis: the bottleneck is SELECTION PRESSURE, not information/drive. Under current night cold, passive (reactive) fire-seeking already suffices to survive, so evolution has no reason to develop anticipatory "lead". Per Ouyang et al. 1998 PNAS (resonating circadian clocks enhance fitness only when the environment is periodic AND harsh enough): to get anticipatory warmth/fire-seeking SELECTED, night cold must be harsh enough that reactive fire-seeking is too late and only anticipation survives. This is an ENV/selection-regime change, NOT reward shaping (no bonus for rhythm/phase/fire).

## Single env knob (one at a time, per §5 discipline)
PRIMARY (this draft): world.survival.cold_temp_threshold 22.0 -> 25.0
- Effect: agents are "cold" (take thermal damage) at a higher core temp, so the cold-damage window covers MORE of each night (observed core_temp avg ~24.6C, so threshold 25 puts agents below it for the colder part of the night). Deepens night cold exposure -> anticipatory warmth-seeking matters more.
- Small step (engine default is 25.0; current scenario lowered it to 22.0). Conservative on purpose.
- NOTHING else changed (thermal_damage_per_second stays 0.3; warmth_recovery, fire damage, day/season length, rewards all untouched).

ALTERNATIVE knob (NOT in this draft; for discussion): world.survival.thermal_damage_per_second 0.3 -> higher
- Effect: being cold kills FASTER = narrows the night survival window directly. Use this instead if threshold change proves too blunt.
(Note: the DayNightLayer night ambient-temperature curve / ambientTemperatureDelta has NO toml key, so it is not a usable single knob without a code change. Hence threshold/damage are the available env levers.)

## Smoke gates (must pass before any short/long pivot run)
- (a) C7 NO-EXTINCTION = HEAD GATE. Harsh night cold may crush the ecology (8 background species + protagonist). gen11/gen12: all 8 species avg_fitness > 0, protagonist survives, NO species extinction. If FAIL -> back off the knob magnitude (smaller step), re-smoke. Prefer small pressure steps over a crushed ecology.
- (b) no crash / no exception in logs.
- (c) harmony 7/7 still fires (build/chop/hunt/signal/craft/deposit/worksite > 0).

## If smoke passes -> short(16) -> long(80), 3 seed x ON/OFF (option 3 co-test)
- Compare gen79 fire-vs-coldtrough harmonic: does anticipation EMERGE under harsher cold?
  - Success = ON (or even OFF) grows lead/R2 ABOVE the old plateau (~R2 0.05-0.13, amp ~0.005, lead 0 on ON).
  - With warmth-drive flag retained (option 3), test whether "harsh env + drive" synergize to produce the proactive lead that neither produced alone.

## Draft artifact
- config: tmp/layer1/DRAFT_harshcold_smoke_50001_on.toml (single knob: cold_temp_threshold 22->25; runs_dir harshcoldD3_smoke_50001_on; warmth_drive ON; rhythm OFF; generations 12; tail_generations 0)
- exe: neural-eco-protagonist-rhythm.exe (already built; production exe untouched)

## Red lines maintained
env change = selection pressure (legal) != reward shaping; one env knob at a time; smoke-first; C7 no-extinction head gate; commit local protagonist_ecology: no push; deep_protagonist untouched; production exe not overwritten; D free >= 200GB (currently 202.1GB).
