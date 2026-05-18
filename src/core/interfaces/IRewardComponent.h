#pragma once
//
// core/interfaces/IRewardComponent.h
//
// A scalar shaping signal added to an agent's fitness each tick. Several
// components stack: total reward = sum of component values * weight.
//
// Components in M1:
//   SurvivalReward    : +x per tick alive
//   FoodReward        : +x per food eaten
//   IntrinsicReward   : +x for novel state visits
//
// Components in F-* modules:
//   CombatReward      : F-FIGHT
//   SharedTaskReward  : F-COOP
//   NoveltyReward     : F-NOVELTY (richer than M1's IntrinsicReward)
//   ...
//
// Why composable rewards instead of one big function?
//   Each module can ship its own shaping component without touching
//   existing reward code. Weights tweak in TOML config.

namespace neuro {

class IAgent;
class IWorld;

class IRewardComponent {
public:
    virtual ~IRewardComponent() = default;

    // Compute reward delta for the given agent at this tick. May read
    // arbitrarily from world; must NOT mutate.
    virtual double evaluate(const IAgent& agent,
                            const IWorld& world,
                            float dt) const = 0;

    // Multiplicative scale (configured via TOML). The runtime adds
    // `weight() * evaluate(...)` to fitness.
    virtual double weight() const = 0;

    // Identifier for logging / debugging.
    virtual const char* name() const = 0;
};

}  // namespace neuro
