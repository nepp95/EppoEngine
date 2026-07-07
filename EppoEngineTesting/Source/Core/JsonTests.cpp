#include "Support/EppoTest.h"

#include "Utility/Json.h"

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

using namespace Eppo;

// Utility/Json.h registers nlohmann adl_serializers for the glm vector types and
// UUID; these round-trip cases guard the (de)serialization used by scene/project
// files.
SUITE(Core)
{
    TEST(JsonVec2RoundTrip)
    {
        const glm::vec2 original(1.5f, -2.5f);
        const nlohmann::json j = original;
        const auto restored = j.get<glm::vec2>();

        CHECK_CLOSE(original.x, restored.x, 1e-6f);
        CHECK_CLOSE(original.y, restored.y, 1e-6f);
    }

    TEST(JsonVec3RoundTrip)
    {
        const glm::vec3 original(1.0f, 2.0f, 3.0f);
        const nlohmann::json j = original;
        const auto restored = j.get<glm::vec3>();

        CHECK_CLOSE(original.x, restored.x, 1e-6f);
        CHECK_CLOSE(original.y, restored.y, 1e-6f);
        CHECK_CLOSE(original.z, restored.z, 1e-6f);
    }

    TEST(JsonVec4RoundTrip)
    {
        const glm::vec4 original(1.0f, 2.0f, 3.0f, 4.0f);
        const nlohmann::json j = original;
        const auto restored = j.get<glm::vec4>();

        CHECK_CLOSE(original.x, restored.x, 1e-6f);
        CHECK_CLOSE(original.y, restored.y, 1e-6f);
        CHECK_CLOSE(original.z, restored.z, 1e-6f);
        CHECK_CLOSE(original.w, restored.w, 1e-6f);
    }

    TEST(JsonVec3SerializesAsArray)
    {
        const glm::vec3 v(4.0f, 5.0f, 6.0f);
        const nlohmann::json j = v;

        CHECK(j.is_array());
        CHECK_EQUAL(3u, j.size());
        CHECK_CLOSE(4.0f, j[0].get<float>(), 1e-6f);
    }

    TEST(JsonUUIDRoundTrip)
    {
        const UUID original(9876543210ull);
        const nlohmann::json j = original;
        const auto restored = j.get<UUID>();

        CHECK(original == restored);
        CHECK_EQUAL(9876543210ull, static_cast<uint64_t>(restored));
    }
}
