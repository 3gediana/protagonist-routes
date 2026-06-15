# Realism Increment 1 (env perception) — ON vs OFF ablation verdict

Two 80-gen runs, identical except the protagonist env-perception flag, same seed 50001,
same compressed timescales (day=200s, season=150s, weather avg=120s, episode=600s):
- ON  : `data/runs/realism_envperc_L1/20260530-125523`  (perception ON)  harmony 7/7
- OFF : `data/runs/realism_envperc_L1_off/20260530-144626` (perception OFF) harmony 7/7

Analysis = protagonists only (named-HRL-goal agents), gen0 vs gen79, episodes 0+1 pooled.
Day phase from `time_seconds` (episode-local) → 3 periodic day-night cycles per episode,
so day-phase effects are separable from monotonic within-episode drift. Season is monotonic
over the episode (drift-confounded) → reported but not used as evidence.

## Decisive comparison — day-phase modulation, gen79 (2 episodes pooled)

| metric | ON gen79 | OFF gen79 |
|---|---|---|
| speed (fast early / slow late) | modul 0.42 | modul 0.45 |
| emit_signal | modul 0.14 | modul 0.22 |
| find_food | modul 0.55 | modul 0.36 |

Per-quarter speed is near-identical: ON [1.72,1.86,1.25,1.22] vs OFF [1.75,1.88,1.21,1.19].

## Honest conclusion (a NEGATIVE result, reported straight)

1. **No robust behavioral signature distinguishes the perception lineage from the blind
   ablation.** The day-night locomotor rhythm (fast in the first half of the day-cycle,
   slow in the second) is present *equally* with and without the perception → it is driven
   by the world's day-night layer acting through channels the agent already senses (food
   distance, etc.), NOT by the new time/season/weather perception inputs.
2. My earlier framing ("evolution amplified the day-night rhythm via the new perception")
   was an ep0 sampling artifact and is **refuted by the ablation + 2-episode pooling**.
   This is exactly why the control run mattered.
3. **Not a wiring bug.** Verified `+10` input dims take effect (resume shape-mismatch guard)
   AND the `sense()` implementations read live, time-varying layer state
   (TimeOfDayPerception → timeOfDay()/lightLevel(); SeasonPerception → season idx/progress;
   WeatherPerception → one-hot currentWeather()). The perception is fed real varying signal.
4. **Interpretation / the real lever.** Supplying perception inputs is *necessary but not
   sufficient*. With the current world there is no environment-specific survival pressure
   that the perception helps solve, so there is no selection gradient to use it. To make
   the (already-wired) perception matter, the WORLD must impose perception-relevant
   pressure — night-specific predation, winter food scarcity / hoarding need, storm
   movement penalties, temperature metabolism cost. That is **Phase B (resource/environment
   realism)**, which is the actual realism lever; Phase A increment 1 is the prerequisite
   plumbing it builds on.

## Caveats on this analysis
- n = 2 episodes/gen; aggregate behaviors only (speed, goal fractions, food distance). A
  perception could be used in subtler/weather-specific ways not captured here.
- Weather is stochastic and NOT yet recorded in agent_trace.csv; adding season/weather/
  time-of-day columns to TraceRecorder is the next plumbing step for ground-truth env
  analysis.

## Decision
KEEP the env-perception wiring (harmless, necessary infra; flag stays, this lineage is the
base for Phase B). PIVOT the realism effort to Phase B: audit which env layers already
impose survival pressure (config) vs which need engine work, then add pressure so the
perception has something to solve. No reward shaping touched.
