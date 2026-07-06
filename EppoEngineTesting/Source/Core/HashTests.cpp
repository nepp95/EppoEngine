#include "Support/EppoTest.h"

#include "Core/Hash.h"

using namespace Eppo;

// Hash::GenerateFnv is used to key cached artifacts (e.g. compiled shaders), so
// it must be deterministic across calls and sensitive to input changes.
SUITE(Core)
{
    TEST(HashIsDeterministic)
    {
        const uint64_t a = Hash::GenerateFnv("the quick brown fox");
        const uint64_t b = Hash::GenerateFnv("the quick brown fox");
        CHECK_EQUAL(a, b);
    }

    TEST(HashDiffersForDifferentInput)
    {
        const uint64_t a = Hash::GenerateFnv("shader_vert");
        const uint64_t b = Hash::GenerateFnv("shader_frag");
        CHECK(a != b);
    }

    TEST(HashIsSensitiveToSmallChange)
    {
        // A single-character difference must produce a different digest.
        const uint64_t a = Hash::GenerateFnv("abc");
        const uint64_t b = Hash::GenerateFnv("abd");
        CHECK(a != b);
    }

    TEST(HashEmptyStringIsStable)
    {
        const uint64_t a = Hash::GenerateFnv("");
        const uint64_t b = Hash::GenerateFnv("");
        CHECK_EQUAL(a, b);
    }
}
