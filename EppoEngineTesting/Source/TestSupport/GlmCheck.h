#pragma once

#include <gtest/gtest.h>

#include <glm/glm.hpp>

// Component-wise EXPECT_NEAR for glm types, so a failure reports which component drifted.

#define CHECK_VEC2_CLOSE(expected, actual, tolerance)                                                                                      \
    do                                                                                                                                     \
    {                                                                                                                                      \
        const glm::vec2 lhs = (expected);                                                                                                  \
        const glm::vec2 rhs = (actual);                                                                                                    \
        EXPECT_NEAR(lhs.x, rhs.x, tolerance);                                                                                              \
        EXPECT_NEAR(lhs.y, rhs.y, tolerance);                                                                                              \
    }                                                                                                                                      \
    while (false)

#define CHECK_VEC3_CLOSE(expected, actual, tolerance)                                                                                      \
    do                                                                                                                                     \
    {                                                                                                                                      \
        const glm::vec3 lhs = (expected);                                                                                                  \
        const glm::vec3 rhs = (actual);                                                                                                    \
        EXPECT_NEAR(lhs.x, rhs.x, tolerance);                                                                                              \
        EXPECT_NEAR(lhs.y, rhs.y, tolerance);                                                                                              \
        EXPECT_NEAR(lhs.z, rhs.z, tolerance);                                                                                              \
    }                                                                                                                                      \
    while (false)

#define CHECK_VEC4_CLOSE(expected, actual, tolerance)                                                                                      \
    do                                                                                                                                     \
    {                                                                                                                                      \
        const glm::vec4 lhs = (expected);                                                                                                  \
        const glm::vec4 rhs = (actual);                                                                                                    \
        EXPECT_NEAR(lhs.x, rhs.x, tolerance);                                                                                              \
        EXPECT_NEAR(lhs.y, rhs.y, tolerance);                                                                                              \
        EXPECT_NEAR(lhs.z, rhs.z, tolerance);                                                                                              \
        EXPECT_NEAR(lhs.w, rhs.w, tolerance);                                                                                              \
    }                                                                                                                                      \
    while (false)

#define CHECK_MAT4_CLOSE(expected, actual, tolerance)                                                                                      \
    do                                                                                                                                     \
    {                                                                                                                                      \
        const glm::mat4 lhs = (expected);                                                                                                  \
        const glm::mat4 rhs = (actual);                                                                                                    \
        for (int col = 0; col < 4; ++col)                                                                                                  \
            CHECK_VEC4_CLOSE(lhs[col], rhs[col], tolerance);                                                                               \
    }                                                                                                                                      \
    while (false)
