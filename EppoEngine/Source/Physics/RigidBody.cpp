#include "pch.h"
#include "RigidBody.h"

#include <bullet/btBulletDynamicsCommon.h>

namespace Eppo
{
	namespace Utils
	{
		static glm::vec3 BulletToGlm(const btVector3& v)
		{
			return { v.getX(), v.getY(), v.getZ() };
		}

		static glm::quat BulletToGlm(const btQuaternion& q)
		{
			return { q.getW(), q.getX(), q.getY(), q.getZ() };
		}

		static btVector3 GlmToBullet(const glm::vec3& v)
		{
			return { v.x, v.y, v.z };
		}

		btQuaternion GlmToBullet(const glm::quat& q)
		{
			return { q.x, q.y, q.z, q.w };
		}
	}

	RigidBody::RigidBody(btRigidBody* body)
		: m_Body(body)
	{}

	void RigidBody::ApplyLinearImpulse(const glm::vec3& impulse, const glm::vec3& worldPosition) const
	{
		EPPO_PROFILE_FUNCTION("RigidBody::ApplyLinearImpulse");
		EPPO_ASSERT(m_Body)

		m_Body->applyImpulse(Utils::GlmToBullet(impulse), Utils::GlmToBullet(worldPosition));
	}

	void RigidBody::ApplyLinearImpulse(const glm::vec3& impulse) const
	{
		EPPO_PROFILE_FUNCTION("RigidBody::ApplyLinearImpulse");
		EPPO_ASSERT(m_Body)

		m_Body->applyCentralImpulse(Utils::GlmToBullet(impulse));
	}

	void RigidBody::ClearBody()
	{
		SetBody(nullptr);
	}
}
