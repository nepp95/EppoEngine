#include "pch.h"
#include "RigidBody.h"

#include <bullet/btBulletDynamicsCommon.h>

namespace Eppo
{
	namespace Utils
	{
		glm::vec3 BulletToGlm(const btVector3& v)
		{
			return glm::vec3(v.getX(), v.getY(), v.getZ());
		}

		glm::quat BulletToGlm(const btQuaternion& q)
		{
			return glm::quat(q.getW(), q.getX(), q.getY(), q.getZ());
		}

		btVector3 GlmToBullet(const glm::vec3& v)
		{
			return btVector3(v.x, v.y, v.z);
		}

		btQuaternion GlmToBullet(const glm::quat& q)
		{
			return btQuaternion(q.x, q.y, q.z, q.w);
		}
	}

	RigidBody::RigidBody(btRigidBody* body)
		: m_Body(body)
	{}

	void RigidBody::ApplyLinearImpulse(const glm::vec3& impulse, const glm::vec3& worldPosition)
	{
		EPPO_PROFILE_FUNCTION("RigidBody::ApplyLinearImpulse");
		EPPO_ASSERT(m_Body);

		m_Body->applyImpulse(Utils::GlmToBullet(impulse), Utils::GlmToBullet(worldPosition));
	}

	void RigidBody::ApplyLinearImpulse(const glm::vec3& impulse)
	{
		EPPO_PROFILE_FUNCTION("RigidBody::ApplyLinearImpulse");
		EPPO_ASSERT(m_Body);

		m_Body->applyCentralImpulse(Utils::GlmToBullet(impulse));
	}

	void RigidBody::ClearBody()
	{
		SetBody(nullptr);
	}
}
