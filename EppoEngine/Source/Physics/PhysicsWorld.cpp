#include "pch.h"
#include "Physics/PhysicsWorld.h"

#include "Physics/PhysicsTypes.h"
#include "Scene/Components.h"

#include <box3d/box3d.h>

namespace Eppo
{
	namespace
	{
		auto ToB3BodyType(RigidBodyComponent::BodyType type) -> b3BodyType
		{
			switch (type)
			{
				case RigidBodyComponent::BodyType::Static:    return b3_staticBody;
				case RigidBodyComponent::BodyType::Kinematic: return b3_kinematicBody;
				case RigidBodyComponent::BodyType::Dynamic:   return b3_dynamicBody;
			}

			EP_ASSERT(false, "Unknown body type!");
			return b3_staticBody;
		}
	}

	PhysicsWorld::PhysicsWorld(const glm::vec3& gravity)
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		worldDef.gravity = ToB3(gravity);
		m_WorldId = b3CreateWorld(&worldDef);
	}

	PhysicsWorld::~PhysicsWorld()
	{
		// Frees all bodies and shapes created in this world.
		b3DestroyWorld(m_WorldId);
	}

	auto PhysicsWorld::CreateBody(const UUID entityId, const RigidBodyComponent& rigidBody, const TransformComponent& transform,
		const BoxColliderComponent* box, const SphereColliderComponent* sphere,
		const CapsuleColliderComponent* capsule) -> void
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = ToB3BodyType(rigidBody.Type);
		bodyDef.position = ToB3(transform.Translation);
		bodyDef.rotation = ToB3(glm::quat(transform.Rotation));
		bodyDef.gravityScale = rigidBody.GravityScale;
		bodyDef.linearDamping = rigidBody.LinearDamping;
		bodyDef.angularDamping = rigidBody.AngularDamping;

		const b3BodyId body = b3CreateBody(m_WorldId, &bodyDef);

		if (box)
		{
			b3ShapeDef shapeDef = b3DefaultShapeDef();
			shapeDef.density = box->Density;
			shapeDef.baseMaterial.friction = box->Friction;
			shapeDef.baseMaterial.restitution = box->Restitution;

			b3BoxHull hull = b3MakeOffsetBoxHull(box->HalfExtents.x, box->HalfExtents.y, box->HalfExtents.z, ToB3(box->Offset));
			b3CreateHullShape(body, &shapeDef, &hull.base);
		}

		if (sphere)
		{
			b3ShapeDef shapeDef = b3DefaultShapeDef();
			shapeDef.density = sphere->Density;
			shapeDef.baseMaterial.friction = sphere->Friction;
			shapeDef.baseMaterial.restitution = sphere->Restitution;

			const b3Sphere s{ ToB3(sphere->Offset), sphere->Radius };
			b3CreateSphereShape(body, &shapeDef, &s);
		}

		if (capsule)
		{
			b3ShapeDef shapeDef = b3DefaultShapeDef();
			shapeDef.density = capsule->Density;
			shapeDef.baseMaterial.friction = capsule->Friction;
			shapeDef.baseMaterial.restitution = capsule->Restitution;

			// Capsule aligned to local Y; Height is the distance between hemisphere centers.
			const float halfHeight = capsule->Height * 0.5f;
			const b3Capsule c{
				ToB3(capsule->Offset - glm::vec3(0.0f, halfHeight, 0.0f)),
				ToB3(capsule->Offset + glm::vec3(0.0f, halfHeight, 0.0f)),
				capsule->Radius
			};
			b3CreateCapsuleShape(body, &shapeDef, &c);
		}

		m_Bodies[entityId] = body;
	}

	auto PhysicsWorld::Step(const float timestep, const int subStepCount) -> void
	{
		b3World_Step(m_WorldId, timestep, subStepCount);
	}

	auto PhysicsWorld::TryGetBody(const UUID entityId, b3BodyId& outBody) const -> bool
	{
		const auto it = m_Bodies.find(entityId);
		if (it == m_Bodies.end())
			return false;

		outBody = it->second;
		return true;
	}

	auto PhysicsWorld::HasBody(const UUID entityId) const -> bool
	{
		return m_Bodies.contains(entityId);
	}

	auto PhysicsWorld::GetPosition(const UUID entityId) const -> glm::vec3
	{
		b3BodyId body;
		if (!TryGetBody(entityId, body))
			return glm::vec3(0.0f);

		return FromB3(b3Body_GetPosition(body));
	}

	auto PhysicsWorld::GetRotation(const UUID entityId) const -> glm::quat
	{
		b3BodyId body;
		if (!TryGetBody(entityId, body))
			return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		return FromB3(b3Body_GetRotation(body));
	}

	auto PhysicsWorld::ApplyLinearImpulse(const UUID entityId, const glm::vec3& impulse) -> void
	{
		b3BodyId body;
		if (!TryGetBody(entityId, body))
		{
			Log::Warn("Physics: ApplyLinearImpulse on entity {} which has no physics body.", static_cast<uint64_t>(entityId));
			return;
		}

		b3Body_ApplyLinearImpulseToCenter(body, ToB3(impulse), true);
	}

	auto PhysicsWorld::GetLinearVelocity(const UUID entityId) const -> glm::vec3
	{
		b3BodyId body;
		if (!TryGetBody(entityId, body))
		{
			Log::Warn("Physics: GetLinearVelocity on entity {} which has no physics body.", static_cast<uint64_t>(entityId));
			return glm::vec3(0.0f);
		}

		return FromB3(b3Body_GetLinearVelocity(body));
	}

	auto PhysicsWorld::SetLinearVelocity(const UUID entityId, const glm::vec3& velocity) -> void
	{
		b3BodyId body;
		if (!TryGetBody(entityId, body))
		{
			Log::Warn("Physics: SetLinearVelocity on entity {} which has no physics body.", static_cast<uint64_t>(entityId));
			return;
		}

		b3Body_SetLinearVelocity(body, ToB3(velocity));
	}
}
