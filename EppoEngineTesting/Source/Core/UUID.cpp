#include "Support/EppoTest.h"

#include <type_traits>

SUITE(Core)
{
    using Eppo::UUID;

    // Compile-time contract: both conversions must stay *explicit*. is_convertible tests
    // implicit convertibility, so these break the day someone drops `explicit` and revives
    // the footgun (a raw id silently collapsing to the bool 0/1) — before any test runs.
    static_assert(!std::is_convertible_v<UUID, bool>, "UUID->bool must be explicit");
    static_assert(!std::is_convertible_v<UUID, uint64_t>, "UUID->uint64_t must be explicit");
    static_assert(std::is_constructible_v<bool, UUID>, "explicit UUID->bool must still work");
    static_assert(std::is_constructible_v<uint64_t, UUID>, "explicit UUID->uint64_t must still work");

    TEST(UUID_BoolConversion_ZeroIsFalse)
    {
        CHECK(!static_cast<bool>(UUID(0)));
    }

    TEST(UUID_BoolConversion_NonZeroIsTrue)
    {
        CHECK(static_cast<bool>(UUID(100)));
        CHECK(static_cast<bool>(UUID(0xFFFFFFFFFFFFFFFFull)));
    }

    TEST(UUID_Uint64Conversion_PreservesValue)
    {
        // The id must survive verbatim, not collapse to a 0/1 bool.
        constexpr uint64_t id = 1234567890123456789ull;
        CHECK_EQUAL(id, static_cast<uint64_t>(UUID(id)));
    }

    TEST(UUID_DefaultConstruction_SkipsReservedRange)
    {
        // Default-constructed ids skip the reserved 1-99 block.
        const UUID id;
        CHECK(static_cast<uint64_t>(id) >= 100);
        CHECK(static_cast<bool>(id));
    }

    TEST(UUID_Equality_ComparesById)
    {
        CHECK(UUID(42) == UUID(42));
        CHECK(UUID(42) != UUID(43));
    }
}
