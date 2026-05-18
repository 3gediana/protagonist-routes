// tests/core/test_vec2.cpp
#include "core/types/Vec2.h"

#include <gtest/gtest.h>

#include <cmath>

using neuro::Vec2;

TEST(Vec2, DefaultConstructIsZero) {
    Vec2 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
}

TEST(Vec2, Arithmetic) {
    Vec2 a{1.0f, 2.0f};
    Vec2 b{3.0f, 4.0f};
    EXPECT_EQ(a + b, Vec2(4.0f, 6.0f));
    EXPECT_EQ(b - a, Vec2(2.0f, 2.0f));
    EXPECT_EQ(a * 2.0f, Vec2(2.0f, 4.0f));
    EXPECT_EQ(b / 2.0f, Vec2(1.5f, 2.0f));
    EXPECT_EQ(-a,        Vec2(-1.0f, -2.0f));
}

TEST(Vec2, CompoundAssignment) {
    Vec2 v{1.0f, 1.0f};
    v += Vec2{2.0f, 3.0f};
    EXPECT_EQ(v, Vec2(3.0f, 4.0f));
    v *= 2.0f;
    EXPECT_EQ(v, Vec2(6.0f, 8.0f));
}

TEST(Vec2, LengthAndNormalize) {
    Vec2 v{3.0f, 4.0f};
    EXPECT_FLOAT_EQ(v.length(), 5.0f);
    EXPECT_FLOAT_EQ(v.lengthSquared(), 25.0f);
    Vec2 n = v.normalized();
    EXPECT_NEAR(n.length(), 1.0f, 1e-6f);
}

TEST(Vec2, NormalizeZeroIsZero) {
    Vec2 z{0.0f, 0.0f};
    Vec2 n = z.normalized();
    EXPECT_FLOAT_EQ(n.x, 0.0f);
    EXPECT_FLOAT_EQ(n.y, 0.0f);
}

TEST(Vec2, DistanceAndDot) {
    Vec2 a{0.0f, 0.0f};
    Vec2 b{3.0f, 4.0f};
    EXPECT_FLOAT_EQ(Vec2::distance(a, b), 5.0f);
    EXPECT_FLOAT_EQ(Vec2::distanceSquared(a, b), 25.0f);
    EXPECT_FLOAT_EQ(Vec2::dot(Vec2(1, 0), Vec2(0, 1)), 0.0f);
    EXPECT_FLOAT_EQ(Vec2::dot(Vec2(1, 2), Vec2(3, 4)), 11.0f);
    EXPECT_FLOAT_EQ(Vec2::cross(Vec2(1, 0), Vec2(0, 1)), 1.0f);
}
