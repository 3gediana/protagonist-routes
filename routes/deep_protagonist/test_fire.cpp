// D-112 mechanical unit smoke for world::FireSystem.
// Compile (from repo root, in a vcvars64 shell):
//   cl /std:c++17 /EHsc /I include test_fire.cpp src/world/FireSystem.cpp /Fe:test_fire.exe
#include "world/FireSystem.hpp"

#include <cstdio>

using dp::world::FireSystem;

int main() {
    int fails = 0;
    auto check = [&](const char* n, bool c) {
        std::printf("[%s] %s\n", c ? "PASS" : "FAIL", n);
        if (!c) ++fails;
    };

    // Default config: warm_radius 6, fuel_per_wood 90, fuel_max 180,
    // warm_rate 45/min, merge_radius 3, max_fires 8.
    {
        FireSystem fs;
        check("build a fire -> tend returns true", fs.tend(0.f, 0.f));
        check("one lit campfire exists", fs.count_lit() == 1);
        check("warmth at center > 0", fs.warm_rate_at(0.f, 0.f) > 0.f);
        check("warmth at 5m (< radius) > 0", fs.warm_rate_at(5.f, 0.f) > 0.f);
        check("NO warmth at 7m (> radius)", fs.warm_rate_at(7.f, 0.f) == 0.f);

        // Within merge_radius -> refuel, no new fire.
        check("refuel within merge -> true", fs.tend(1.f, 0.f));
        check("still ONE fire after refuel", fs.count_lit() == 1);
        // Far away -> a brand new fire.
        check("far tend -> 2nd fire", (fs.tend(20.f, 0.f), fs.count_lit() == 2));
    }

    // Burn-down + extinguish.
    {
        FireSystem fs;
        fs.tend(0.f, 0.f);                       // fuel = 90s
        for (int i = 0; i < 89; ++i) fs.tick(1.f);
        check("still lit at 89s", fs.count_lit() == 1 && fs.warm_rate_at(0.f, 0.f) > 0.f);
        fs.tick(2.f);                            // crosses 90s -> burnt out
        check("extinguished after fuel exhausted", fs.count_lit() == 0);
        check("NO warmth after burnout", fs.warm_rate_at(0.f, 0.f) == 0.f);
    }

    // Refuel cap: a full fire rejects more wood (so the caller wastes none).
    {
        FireSystem fs;
        check("first build -> true", fs.tend(0.f, 0.f));    // 90
        check("refuel to cap -> true",  fs.tend(0.5f, 0.f)); // 180 (cap)
        check("refuel when FULL -> false (no wood waste)", !fs.tend(0.5f, 0.f));
    }

    std::printf("\n%s  (fails=%d)\n", fails == 0 ? "ALL PASS" : "FAILURES", fails);
    return fails;
}
