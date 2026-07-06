#pragma once

#include <UnitTest++/UnitTest++.h>

#include <glm/glm.hpp>

// Component-wise CHECK_CLOSE helpers for glm types. Comparing vectors/matrices
// element by element means a failure reports exactly which component drifted and
// by how much, instead of an opaque "objects differ".

#define CHECK_VEC2_CLOSE(expected, actual, tolerance)              \
    do {                                                           \
        const glm::vec2 _epExpected = (expected);                  \
        const glm::vec2 _epActual = (actual);                      \
        CHECK_CLOSE(_epExpected.x, _epActual.x, tolerance);        \
        CHECK_CLOSE(_epExpected.y, _epActual.y, tolerance);        \
    } while (false)

#define CHECK_VEC3_CLOSE(expected, actual, tolerance)              \
    do {                                                           \
        const glm::vec3 _epExpected = (expected);                  \
        const glm::vec3 _epActual = (actual);                      \
        CHECK_CLOSE(_epExpected.x, _epActual.x, tolerance);        \
        CHECK_CLOSE(_epExpected.y, _epActual.y, tolerance);        \
        CHECK_CLOSE(_epExpected.z, _epActual.z, tolerance);        \
    } while (false)

#define CHECK_VEC4_CLOSE(expected, actual, tolerance)              \
    do {                                                           \
        const glm::vec4 _epExpected = (expected);                  \
        const glm::vec4 _epActual = (actual);                      \
        CHECK_CLOSE(_epExpected.x, _epActual.x, tolerance);        \
        CHECK_CLOSE(_epExpected.y, _epActual.y, tolerance);        \
        CHECK_CLOSE(_epExpected.z, _epActual.z, tolerance);        \
        CHECK_CLOSE(_epExpected.w, _epActual.w, tolerance);        \
    } while (false)

#define CHECK_MAT4_CLOSE(expected, actual, tolerance)             \
    do {                                                          \
        const glm::mat4 _epExpectedM = (expected);                \
        const glm::mat4 _epActualM = (actual);                    \
        for (int _epCol = 0; _epCol < 4; ++_epCol)                \
            CHECK_VEC4_CLOSE(_epExpectedM[_epCol], _epActualM[_epCol], tolerance); \
    } while (false)
