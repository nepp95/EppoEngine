#pragma once

#include "Core/UUID.h"

#include <box3d/id.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <unordered_map>
#include <vector>

namespace Eppo
{
	struct RigidBodyComponent;

	enum class ColliderShape : uint8_t { Box, Sphere, Capsule, Cylinder };

	// Shape-agnostic collider description: the scene translates each collider
	// component into one of these, so adding a shape never changes CreateBody's
	// signature (add an enum value + a case in AttachCollider).
	struct ColliderData
	{
		ColliderShape Shape = ColliderShape::Box;
		// Body-local shape pose; Rotation orients the shape but does not re-rotate Offset.
		glm::vec3 Offset = glm::vec3(0.0f);
		glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;

		glm::vec3 HalfExtents = glm::vec3(0.5f); // Box
		float Radius = 0.5f;                     // Sphere, Capsule
		float Height = 1.0f;                     // Capsule
	};

	class PhysicsWorld
	{
	public:
		explicit PhysicsWorld(const glm::vec3& gravity);
		~PhysicsWorld();

		PhysicsWorld(const PhysicsWorld&) = delete;
		PhysicsWorld& operator=(const PhysicsWorld&) = delete;

		auto CreateBody(UUID entityId, const RigidBodyComponent& rigidBody, const glm::vec3& position, const glm::quat& rotation,
			const std::vector<ColliderData>& colliders) -> void;

		auto Step(float timestep, int subStepCount = 4) -> void;

		[[nodiscard]] auto HasBody(UUID entityId) const -> bool;
		[[nodiscard]] auto GetShapeCount(UUID entityId) const -> int;

		[[nodiscard]] auto GetPosition(UUID entityId) const -> glm::vec3;
		[[nodiscard]] auto GetRotation(UUID entityId) const -> glm::quat;

		auto ApplyLinearImpulse(UUID entityId, const glm::vec3& impulse) -> void;
		[[nodiscard]] auto GetLinearVelocity(UUID entityId) const -> glm::vec3;
		auto SetLinearVelocity(UUID entityId, const glm::vec3& velocity) -> void;

	private:
		auto AttachCollider(b3BodyId body, const ColliderData& collider) const -> void;
		[[nodiscard]] auto TryGetBody(UUID entityId, b3BodyId& outBody) const -> bool;

		b3WorldId m_WorldId;
		std::unordered_map<UUID, b3BodyId> m_Bodies;
	};
}
