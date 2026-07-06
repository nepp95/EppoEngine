#include "Support/EppoTest.h"
#include "Support/GlmCheck.h"

#include "Scene/Components.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace Eppo;

// TransformComponent::GetTransform() composes translate * rotate * scale, so a
// local point is scaled first, then rotated, then translated.
SUITE(Scene)
{
    constexpr float kTol = 1e-5f;

    TEST(TransformDefaultIsIdentity)
    {
        const TransformComponent transform;
        CHECK_MAT4_CLOSE(glm::mat4(1.0f), transform.GetTransform(), kTol);
    }

    TEST(TransformPureTranslation)
    {
        TransformComponent transform;
        transform.Translation = { 1.0f, 2.0f, 3.0f };

        const glm::mat4 expected = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
        CHECK_MAT4_CLOSE(expected, transform.GetTransform(), kTol);
    }

    TEST(TransformPureScale)
    {
        TransformComponent transform;
        transform.Scale = { 2.0f, 3.0f, 4.0f };

        const glm::mat4 expected = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 3.0f, 4.0f));
        CHECK_MAT4_CLOSE(expected, transform.GetTransform(), kTol);
    }

    TEST(TransformRotationRotatesPoint)
    {
        TransformComponent transform;
        transform.Rotation = { 0.0f, 0.0f, glm::radians(90.0f) };

        // +90 deg about Z sends the +X axis onto +Y.
        const glm::vec4 rotated = transform.GetTransform() * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        CHECK_VEC3_CLOSE(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(rotated), kTol);
    }

    TEST(TransformScaleThenTranslateOrder)
    {
        TransformComponent transform;
        transform.Translation = { 10.0f, 0.0f, 0.0f };
        transform.Scale = { 2.0f, 2.0f, 2.0f };

        // Local (1,1,1) scales to (2,2,2), then translates to (12,2,2).
        const glm::vec4 result = transform.GetTransform() * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        CHECK_VEC3_CLOSE(glm::vec3(12.0f, 2.0f, 2.0f), glm::vec3(result), kTol);
    }
}
