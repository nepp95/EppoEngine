#pragma once

#include "Core/UUID.h"

#include <box3d/id.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <unordered_map>

namespace Eppo
{
	struct RigidBodyComponent;
	struct TransformComponent;
	struct BoxColliderComponent;
	struct SphereColliderComponent;
	struct CapsuleColliderComponent;

	// Thin RAII wrapper around a Box3D world. Owns the world and the UUID->body
	// map so the rest of the engine never touches Box3D handles directly (same
	// containment discipline as Platform/Vulkan). Lives only during play: the
	// scene creates one in OnRuntimeStart and destroys it in OnRuntimeStop.
	class PhysicsWorld
	{
	public:
		explicit PhysicsWorld(const glm::vec3& gravity);
		~PhysicsWorld();

		PhysicsWorld(const PhysicsWorld&) = delete;
		PhysicsWorld& operator=(const PhysicsWorld&) = delete;

		// Builds a body for the entity from its rigid-body + collider components.
		// Any collider pointer may be null; a body with no collider is inert.
		auto CreateBody(UUID entityId, const RigidBodyComponent& rigidBody, const TransformComponent& transform,
			const BoxColliderComponent* box, const SphereColliderComponent* sphere,
			const CapsuleColliderComponent* capsule) -> void;

		auto Step(float timestep, int subStepCount = 4) -> void;

		[[nodiscard]] auto HasBody(UUID entityId) const -> bool;

		// Simulated pose, for writing back into the TransformComponent each frame.
		[[nodiscard]] auto GetPosition(UUID entityId) const -> glm::vec3;
		[[nodiscard]] auto GetRotation(UUID entityId) const -> glm::quat;

		// Script bridge. Miss (unknown UUID / no body) logs and no-ops.
		auto ApplyLinearImpulse(UUID entityId, const glm::vec3& impulse) -> void;
		[[nodiscard]] auto GetLinearVelocity(UUID entityId) const -> glm::vec3;
		auto SetLinearVelocity(UUID entityId, const glm::vec3& velocity) -> void;

	private:
		// Resolves entityId to its body handle, false if the entity has none.
		[[nodiscard]] auto TryGetBody(UUID entityId, b3BodyId& outBody) const -> bool;

		b3WorldId m_WorldId;
		std::unordered_map<UUID, b3BodyId> m_Bodies;
	};
}
