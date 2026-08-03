#include "Support/EppoTest.h"

#include <type_traits>

using Eppo::UUID;

// Compile-time contract: both conversions must stay *explicit*. is_convertible tests
// implicit convertibility, so these break the day someone drops `explicit` and revives
// the footgun (a raw id silently collapsing to the bool 0/1) — before any test runs.
static_assert(!std::is_convertible_v<UUID, bool>, "UUID->bool must be explicit");
static_assert(!std::is_convertible_v<UUID, uint64_t>, "UUID->uint64_t must be explicit");
static_assert(std::is_constructible_v<bool, UUID>, "explicit UUID->bool must still work");
static_assert(std::is_constructible_v<uint64_t, UUID>, "explicit UUID->uint64_t must still work");

TEST(Core, UUID_BoolConversion_ZeroIsFalse)
{
    EXPECT_TRUE(!static_cast<bool>(UUID(0)));
}

TEST(Core, UUID_BoolConversion_NonZeroIsTrue)
{
    EXPECT_TRUE(static_cast<bool>(UUID(100)));
    EXPECT_TRUE(static_cast<bool>(UUID(0xFFFFFFFFFFFFFFFFull)));
}

TEST(Core, UUID_Uint64Conversion_PreservesValue)
{
    // The id must survive verbatim, not collapse to a 0/1 bool.
    constexpr uint64_t id = 1234567890123456789ull;
    EXPECT_EQ(id, static_cast<uint64_t>(UUID(id)));
}

TEST(Core, UUID_DefaultConstruction_SkipsReservedRange)
{
    // Default-constructed ids skip the reserved 1-99 block.
    const UUID id;
    EXPECT_TRUE(static_cast<uint64_t>(id) >= 100);
    EXPECT_TRUE(static_cast<bool>(id));
}

TEST(Core, UUID_Equality_ComparesById)
{
    EXPECT_TRUE(UUID(42) == UUID(42));
    EXPECT_TRUE(UUID(42) != UUID(43));
}
