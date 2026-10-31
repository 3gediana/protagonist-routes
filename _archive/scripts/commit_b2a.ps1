cd D:\claude-code\c++
git add routes/protagonist_ecology/runtime/ProtagonistEcologyConfig.h routes/protagonist_ecology/runtime/ProtagonistEcologyConfig.cpp routes/protagonist_ecology/runtime/ProtagonistWorldFactory.cpp
git status --short -- routes/protagonist_ecology/runtime
$msg = "protagonist_ecology realism Phase B #2a: expose env survival-pressure knobs (cold/hot temp threshold, thermal_damage, hunger threshold/damage) via world.survival.* config, wired into ProtagonistWorldFactory. Defaults reproduce prior hardcoded ctor values exactly => zero behavior change unless overridden. Lets winter cold / hunger pressure be tuned per-toml without recompiling (Phase B #2b). Build clean."
git commit -m $msg
git log --oneline -1
