#pragma once

#include <glm/glm.hpp>

class btRigidBody;

namespace Eppo
{
	class RigidBody
	{
	public:
		RigidBody(btRigidBody* body);
		RigidBody() = default;

		void ApplyLinearImpulse(const glm::vec3& impulse, const glm::vec3& worldPosition);
		void ApplyLinearImpulse(const glm::vec3& impulse);

		btRigidBody* GetBody() const { return m_Body; }
		void SetBody(btRigidBody* body) { m_Body = body; }
		void ClearBody();

	private:
		btRigidBody* m_Body;
	};
}
