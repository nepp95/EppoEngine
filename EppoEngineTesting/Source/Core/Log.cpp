#include "TestSupport/EppoTest.h"

#include "Core/Log.h"
#include "Core/UUID.h"

#include <filesystem>

using Eppo::UUID;

// Log::Init and the routing loggers are process-global and out of scope (see the plan);
// what is pure and worth pinning are the two fmt formatters Log.h defines.

TEST(Core, LogFormatter_Path_UsesStringRepresentation)
{
    const std::filesystem::path path = std::filesystem::path("Resources") / "Shaders" / "geometry.slang";
    EXPECT_EQ(path.string(), fmt::format("{}", path));
}

TEST(Core, LogFormatter_UUID_FormatsAsDecimalId)
{
    const UUID uuid(1234567890123456789ULL);
    EXPECT_EQ("1234567890123456789", fmt::format("{}", uuid));
}
