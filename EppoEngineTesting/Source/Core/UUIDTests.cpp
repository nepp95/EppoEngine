#include "Support/EppoTest.h"

#include "Core/UUID.h"

#include <unordered_map>
#include <unordered_set>

using namespace Eppo;

// UUID's default constructor draws a random 64-bit value, reserving 1-99 for
// engine-internal handles, so every default-constructed id is >= 100 and truthy.
SUITE(Core)
{
    TEST(UUIDDefaultConstructedIsNonReserved)
    {
        for (int i = 0; i < 1000; ++i)
        {
            const UUID id;
            CHECK(static_cast<uint64_t>(id) >= 100);
            CHECK(static_cast<bool>(id));
        }
    }

    TEST(UUIDDefaultConstructedAreUnique)
    {
        std::unordered_set<uint64_t> seen;
        for (int i = 0; i < 10000; ++i)
            seen.insert(static_cast<uint64_t>(UUID()));

        // Collisions across 10k draws from a 64-bit space are astronomically
        // unlikely; a duplicate signals a broken generator, not bad luck.
        CHECK_EQUAL(10000u, seen.size());
    }

    TEST(UUIDExplicitValueIsPreserved)
    {
        const UUID id(123456789ull);
        CHECK_EQUAL(123456789ull, static_cast<uint64_t>(id));
    }

    TEST(UUIDZeroIsFalsy)
    {
        const UUID zero(0);
        CHECK(!static_cast<bool>(zero));

        const UUID nonZero(1);
        CHECK(static_cast<bool>(nonZero));
    }

    TEST(UUIDEqualityAndInequality)
    {
        const UUID a(42);
        const UUID b(42);
        const UUID c(43);

        CHECK(a == b);
        CHECK(!(a == c));
        CHECK(a != c);
        CHECK(!(a != b));
    }

    TEST(UUIDOrdering)
    {
        const UUID small(10);
        const UUID large(20);

        CHECK(small < large);
        CHECK(!(large < small));
        CHECK(!(small < small));
    }

    TEST(UUIDIsUsableAsMapKey)
    {
        std::unordered_map<UUID, int> map;
        const UUID key(7);
        map[key] = 99;

        CHECK_EQUAL(1u, map.count(key));
        CHECK_EQUAL(99, map[UUID(7)]);
        CHECK_EQUAL(0u, map.count(UUID(8)));
    }
}
