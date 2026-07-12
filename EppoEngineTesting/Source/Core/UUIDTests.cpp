#include "Support/EppoTest.h"

#include "Core/UUID.h"

#include <type_traits>

// Compile-time contract: both conversions must stay *explicit*. is_convertible tests
// implicit convertibility, so these break the day someone drops `explicit` and revives
// the footgun (a raw id silently collapsing to the bool 0/1) — before any test runs.
static_assert(!std::is_convertible_v<Eppo::UUID, bool>, "UUID->bool must be explicit");
static_assert(!std::is_convertible_v<Eppo::UUID, uint64_t>, "UUID->uint64_t must be explicit");
static_assert(std::is_constructible_v<bool, Eppo::UUID>, "explicit UUID->bool must still work");
static_assert(std::is_constructible_v<uint64_t, Eppo::UUID>, "explicit UUID->uint64_t must still work");

// The runtime suite locks in the behavior behind those explicit conversions.
SUITE(UUID)
{
    using Eppo::UUID;

    TEST(ZeroIsFalsy)
    {
        CHECK(!static_cast<bool>(UUID(0)));
    }

    TEST(NonZeroIsTruthy)
    {
        CHECK(static_cast<bool>(UUID(100)));
        CHECK(static_cast<bool>(UUID(0xFFFFFFFFFFFFFFFFull)));
    }

    TEST(Uint64RoundTripsExactly)
    {
        // The id must survive verbatim, not collapse to a 0/1 bool.
        constexpr uint64_t id = 1234567890123456789ull;
        CHECK_EQUAL(id, static_cast<uint64_t>(UUID(id)));
    }

    TEST(DefaultSkipsReservedRangeAndIsTruthy)
    {
        // Default-constructed ids skip the reserved 1-99 block, so are always truthy.
        const UUID id;
        CHECK(static_cast<uint64_t>(id) >= 100);
        CHECK(static_cast<bool>(id));
    }

    TEST(EqualityComparesById)
    {
        CHECK(UUID(42) == UUID(42));
        CHECK(UUID(42) != UUID(43));
    }
}
