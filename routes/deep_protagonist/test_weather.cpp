// Stage W1 world coherence — mechanical unit smoke for the rain coupling
// chain primitives (WeatherSystem state machine, FireSystem::apply_rain douse,
// FarmingSystem::rain water top-up). These are libtorch-free so they compile
// and run in seconds.
//
// Compile (from repo root, in a vcvars64 shell):
//   cl /std:c++20 /EHsc /I include test_weather.cpp ^
//      src/world/WeatherSystem.cpp src/world/FireSystem.cpp ^
//      src/world/FarmingSystem.cpp /Fe:test_weather.exe
#include "world/WeatherSystem.hpp"
#include "world/FireSystem.hpp"
#include "world/FarmingSystem.hpp"

#include <cstdio>
#include <functional>

using dp::world::WeatherSystem;
using dp::world::FireSystem;
using dp::world::FarmingSystem;

int main() {
    int fails = 0;
    auto check = [&](const char* n, bool c) {
        std::printf("[%s] %s\n", c ? "PASS" : "FAIL", n);
        if (!c) ++fails;
    };

    const float dt = 1.0f / 30.0f;  // 30 Hz, matches the trainer fixed_dt

    // ---- (1) WeatherSystem DISABLED => sky permanently clear -------------
    {
        WeatherSystem w;  // default config: enabled = false
        bool ever_rained = false;
        float max_inten = 0.0f;
        for (int i = 0; i < 30 * 600; ++i) {   // 10 simulated minutes
            w.tick(dt);
            if (w.raining()) ever_rained = true;
            if (w.intensity() > max_inten) max_inten = w.intensity();
        }
        check("disabled: never rains over 10 min", !ever_rained);
        check("disabled: intensity stays 0", max_inten == 0.0f);
    }

    // ---- (2) WeatherSystem ENABLED => it rains, intensity in [0,1] -------
    {
        WeatherSystem::Config c;
        c.enabled = true;
        c.rain_prob_per_min = 0.95f;   // very likely to rain within minutes
        c.rain_dur_s = 60.0f;
        WeatherSystem w(c, 12345);
        bool ever_rained = false;
        bool inten_in_range = true;
        for (int i = 0; i < 30 * 600; ++i) {
            w.tick(dt);
            if (w.raining()) ever_rained = true;
            float in = w.intensity();
            if (in < 0.0f || in > 1.0f) inten_in_range = false;
        }
        check("enabled: rains at least once in 10 min", ever_rained);
        check("enabled: intensity always within [0,1]", inten_in_range);

        // reset returns to a clear sky.
        w.reset(999);
        check("reset: clear right after reset", !w.raining());
        check("reset: intensity 0 right after reset", w.intensity() == 0.0f);
    }

    // ---- (3) Rain douses an EXPOSED fire, spares a roofed one ------------
    {
        FireSystem fs;
        fs.tend(0.f, 0.f);                       // one lit fire, fuel = 90s
        check("fire lit before rain", fs.count_lit() == 1);
        // exposed() == true everywhere -> heavy douse drains the fuel fast.
        std::function<bool(float,float)> exposed_all =
            [](float, float) { return true; };
        for (int i = 0; i < 30 * 20; ++i) {      // 20 s of douse @ 6/s
            fs.apply_rain(dt, 6.0f, exposed_all);
            fs.tick(dt);
        }
        check("exposed fire is doused out by rain", fs.count_lit() == 0);

        FireSystem fs2;
        fs2.tend(0.f, 0.f);
        // exposed() == false everywhere (roofed / desert) -> rain can't touch it.
        std::function<bool(float,float)> exposed_none =
            [](float, float) { return false; };
        for (int i = 0; i < 30 * 20; ++i) {
            fs2.apply_rain(dt, 6.0f, exposed_none);
            fs2.tick(dt);
        }
        check("roofed fire survives the same rain", fs2.count_lit() == 1);
    }

    // ---- (4) Rain tops up farm plots' water_level ("雨补水") --------------
    {
        FarmingSystem farm;
        int id = farm.plant_seed(0.f, 0.f, 0.f, dp::world::PlantKind::Berry);
        check("planted a seed", id >= 0 && farm.total_count() == 1);
        float w0 = farm.plots()[0].water_level;   // starts ~0.5
        // 5 s of rain at 0.10/s would add 0.5 -> clamps at 1.0.
        for (int i = 0; i < 30 * 5; ++i) farm.rain(dt, 0.10f);
        float w1 = farm.plots()[0].water_level;
        check("rain raised plot water_level", w1 > w0);
        check("plot water_level clamped to 1.0", w1 <= 1.0001f);

        // per_s <= 0 is a no-op.
        FarmingSystem farm2;
        farm2.plant_seed(0.f, 0.f, 0.f, dp::world::PlantKind::Berry);
        float b0 = farm2.plots()[0].water_level;
        farm2.rain(dt, 0.0f);
        check("rain(per_s=0) is a no-op", farm2.plots()[0].water_level == b0);
    }

    std::printf("\n%s (%d failure%s)\n",
                fails == 0 ? "ALL PASS" : "SOME FAILED",
                fails, fails == 1 ? "" : "s");
    return fails == 0 ? 0 : 1;
}
