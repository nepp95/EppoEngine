#pragma once

#include <glm/glm.hpp>

class btRigidBody;

namespace Eppo
{
    struct RigidBody
    {
        btRigidBody* Body;

        explicit RigidBody(btRigidBody* body);
        RigidBody() = default;

        void ApplyLinearImpulse(const glm::vec3& impulse, const glm::vec3& worldPosition) const;
        void ApplyLinearImpulse(const glm::vec3& impulse) const;
    };
}
