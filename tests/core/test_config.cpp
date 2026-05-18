// tests/core/test_config.cpp
#include "core/config/Config.h"

#include <gtest/gtest.h>

using neuro::config::Config;

namespace {

constexpr const char* kSample = R"(
[runtime]
mode = "headless"
random_seed = 42
log_level = "debug"

[world]
size = [200.0, 150.0]

[evolution]
generations = 100

[evolution.mutation]
add_node_rate = 0.03
weight_perturb_sigma = 0.5
)";

}

TEST(Config, GetScalarTypes) {
    Config cfg = Config::loadFromString(kSample);
    EXPECT_EQ(cfg.get<std::string>("runtime.mode"), "headless");
    EXPECT_EQ(cfg.get<int>("runtime.random_seed"), 42);
    EXPECT_EQ(cfg.get<std::string>("runtime.log_level"), "debug");
    EXPECT_EQ(cfg.get<int>("evolution.generations"), 100);
    EXPECT_FLOAT_EQ(cfg.get<float>("evolution.mutation.add_node_rate"), 0.03f);
    EXPECT_DOUBLE_EQ(cfg.get<double>("evolution.mutation.weight_perturb_sigma"), 0.5);
}

TEST(Config, GetArray) {
    Config cfg = Config::loadFromString(kSample);
    auto sizes = cfg.getArray<double>("world.size");
    ASSERT_EQ(sizes.size(), 2u);
    EXPECT_DOUBLE_EQ(sizes[0], 200.0);
    EXPECT_DOUBLE_EQ(sizes[1], 150.0);
}

TEST(Config, MissingKeyReturnsNullopt) {
    Config cfg = Config::loadFromString(kSample);
    EXPECT_FALSE(cfg.tryGet<int>("missing.key").has_value());
    EXPECT_FALSE(cfg.has("missing.key"));
}

TEST(Config, MissingKeyThrowsOnGet) {
    Config cfg = Config::loadFromString(kSample);
    EXPECT_THROW(cfg.get<int>("missing.key"), std::runtime_error);
}

TEST(Config, GetOrFallsBack) {
    Config cfg = Config::loadFromString(kSample);
    EXPECT_EQ(cfg.getOr<int>("missing.key", 99), 99);
    EXPECT_EQ(cfg.getOr<std::string>("runtime.mode", "fallback"), "headless");
}

TEST(Config, IntToFloatCoercion) {
    Config cfg = Config::loadFromString("[a]\nb = 5\n");
    EXPECT_FLOAT_EQ(cfg.get<float>("a.b"), 5.0f);
    EXPECT_DOUBLE_EQ(cfg.get<double>("a.b"), 5.0);
}

TEST(Config, ParseFailureThrows) {
    EXPECT_THROW(Config::loadFromString("[unclosed"), std::runtime_error);
}

TEST(Config, NestedTablesResolve) {
    constexpr const char* deep = R"(
[a.b.c.d]
e = "deep"
)";
    Config cfg = Config::loadFromString(deep);
    EXPECT_EQ(cfg.get<std::string>("a.b.c.d.e"), "deep");
}

TEST(Config, MissingFileThrows) {
    EXPECT_THROW(Config::loadFromFile("definitely_does_not_exist_xyz.toml"),
                 std::runtime_error);
}
