#include "pch.h"
#include "RigidBody.h"

#include <bullet/btBulletDynamicsCommon.h>

namespace Eppo
{
    namespace
    {
        glm::vec3 BulletToGlm(const btVector3& v)
        {
            return { v.getX(), v.getY(), v.getZ() };
        }

        glm::quat BulletToGlm(const btQuaternion& q)
        {
            return { q.getW(), q.getX(), q.getY(), q.getZ() };
        }

        btVector3 GlmToBullet(const glm::vec3& v)
        {
            return { v.x, v.y, v.z };
        }

        btQuaternion GlmToBullet(const glm::quat& q)
        {
            return { q.x, q.y, q.z, q.w };
        }
    }

    RigidBody::RigidBody(btRigidBody* body)
        : Body(body)
    {}

    void RigidBody::ApplyLinearImpulse(const glm::vec3& impulse, const glm::vec3& worldPosition) const
    {
        EPPO_PROFILE_FUNCTION("RigidBody::ApplyLinearImpulse");
        EPPO_ASSERT(Body);

        Body->applyImpulse(GlmToBullet(impulse), GlmToBullet(worldPosition));
    }

    void RigidBody::ApplyLinearImpulse(const glm::vec3& impulse) const
    {
        EPPO_PROFILE_FUNCTION("RigidBody::ApplyLinearImpulse");
        EPPO_ASSERT(Body);

        Body->applyCentralImpulse(GlmToBullet(impulse));
    }
}
