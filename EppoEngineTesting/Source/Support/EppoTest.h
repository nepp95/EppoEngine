#pragma once

// Common test prelude — include this first in every test .cpp.
//
// Engine headers rely on EppoEngine's precompiled header (Source/pch.h) for the
// standard-library types they name in signatures; test translation units do not
// get that PCH, so we pull it in explicitly here. It also brings in the core
// aliases (Ref/ScopedPtr), UUID, Buffer and utility helpers. UnitTest++ supplies
// the TEST / SUITE / CHECK macros.
//
// Conventions for tests in this target:
//   * One SUITE per engine subsystem (Core, Scene, Scripting, Input).
//   * One file per unit under test, named <Thing>Tests.cpp.
//   * Use TEST for stateless cases and TEST_FIXTURE for shared setup/teardown.
//   * Reach for the helpers in Support/ (TempDir, CHECK_*_CLOSE) rather than
//     re-rolling filesystem scratch space or component-wise glm comparisons.
#include "pch.h"

#include <UnitTest++/UnitTest++.h>
