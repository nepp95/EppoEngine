#pragma once

#include "pch.h" // Engine headers below rely on the PCH for Ref<>, std includes, Log, etc.

#include <UnitTest++/UnitTest++.h>

// REQUIRE CHECK(expr): a hard assertion that aborts the current test on failure,
// so later lines can assume the precondition held (UnitTest++'s CHECK alone only
// records the failure and continues).
