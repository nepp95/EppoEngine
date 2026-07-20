#include "pch.h"
#include "Physics/PhysicsWorld.h"

#include "Physics/PhysicsTypes.h"
#include "Scene/Components.h"

#include <box3d/box3d.h>

namespace Eppo
{
	namespace Utils
	{
		static auto ToB3BodyType(RigidBodyComponent::BodyType type) -> b3BodyType
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
		worldDef.gravity = Utils::ToB3(gravity);
		m_WorldId = b3CreateWorld(&worldDef);
	}

	PhysicsWorld::~PhysicsWorld()
	{
		b3DestroyWorld(m_WorldId);
	}

	auto PhysicsWorld::CreateBody(const UUID entityId, const RigidBodyComponent& rigidBody, const glm::vec3& position, const glm::quat& rotation,
		const std::vector<ColliderData>& colliders) -> void
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = Utils::ToB3BodyType(rigidBody.Type);
		bodyDef.position = Utils::ToB3(position);
		bodyDef.rotation = Utils::ToB3(rotation);
		bodyDef.gravityScale = rigidBody.GravityScale;
		bodyDef.linearDamping = rigidBody.LinearDamping;
		bodyDef.angularDamping = rigidBody.AngularDamping;

		const b3BodyId body = b3CreateBody(m_WorldId, &bodyDef);
		// Tag the body with its entity UUID so world queries can map a shape back
		// to the entity that owns it (shape -> body -> userData).
		b3Body_SetUserData(body, reinterpret_cast<void*>(static_cast<uintptr_t>(entityId)));
		for (const ColliderData& collider : colliders)
			AttachCollider(body, collider);
		if (colliders.empty() && rigidBody.Type == RigidBodyComponent::BodyType::Dynamic)
		{
			b3MassData massData{};
			massData.mass = 1.0f;
			b3Body_SetMassData(body, massData);
		}

		m_Bodies[entityId] = body;
	}

	auto PhysicsWorld::AttachCollider(const b3BodyId body, const ColliderData& collider) const -> void
	{
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = collider.Density;
		shapeDef.baseMaterial.friction = collider.Friction;
		shapeDef.baseMaterial.restitution = collider.Restitution;

		switch (collider.Shape)
		{
			case ColliderShape::Box:
			{
				const b3Transform pose{ Utils::ToB3(collider.Offset), Utils::ToB3(collider.Rotation) };
				b3BoxHull hull = b3MakeTransformedBoxHull(collider.HalfExtents.x, collider.HalfExtents.y, collider.HalfExtents.z, pose);
				b3CreateHullShape(body, &shapeDef, &hull.base);
				break;
			}

			case ColliderShape::Sphere:
			{
				const b3Sphere sphere{ Utils::ToB3(collider.Offset), collider.Radius };
				b3CreateSphereShape(body, &shapeDef, &sphere);
				break;
			}

			case ColliderShape::Capsule:
			{
				// Axis is local Y rotated by the shape rotation; Height spans the hemisphere centers.
				const glm::vec3 halfAxis = collider.Rotation * glm::vec3(0.0f, collider.Height * 0.5f, 0.0f);
				const b3Capsule capsule{
					Utils::ToB3(collider.Offset - halfAxis),
					Utils::ToB3(collider.Offset + halfAxis),
					collider.Radius
				};
				b3CreateCapsuleShape(body, &shapeDef, &capsule);
				break;
			}

			case ColliderShape::Cylinder:
			{
				b3HullData* hull = b3CreateCylinder(collider.Height, collider.Radius, -collider.Height * 0.5f, 16);
				const b3Transform pose{ Utils::ToB3(collider.Offset), Utils::ToB3(collider.Rotation) };
				b3CreateTransformedHullShape(body, &shapeDef, hull, pose, b3Vec3{ 1.0f, 1.0f, 1.0f });
				b3DestroyHull(hull);
				break;
			}
		}
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

	auto PhysicsWorld::GetShapeCount(const UUID entityId) const -> int
	{
		b3BodyId body;
		if (!TryGetBody(entityId, body))
			return 0;

		return b3Body_GetShapeCount(body);
	}

	auto PhysicsWorld::GetPosition(const UUID entityId) const -> glm::vec3
	{
		b3BodyId body;
		if (!TryGetBody(entityId, body))
			return glm::vec3(0.0f);

		return Utils::FromB3(b3Body_GetPosition(body));
	}

	auto PhysicsWorld::GetRotation(const UUID entityId) const -> glm::quat
	{
		b3BodyId body;
		if (!TryGetBody(entityId, body))
			return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		return Utils::FromB3(b3Body_GetRotation(body));
	}

	auto PhysicsWorld::ApplyLinearImpulse(const UUID entityId, const glm::vec3& impulse) -> void
	{
		b3BodyId body;
		if (!TryGetBody(entityId, body))
		{
			Log::Warn("Physics: ApplyLinearImpulse on entity {} which has no physics body.", static_cast<uint64_t>(entityId));
			return;
		}

		b3Body_ApplyLinearImpulseToCenter(body, Utils::ToB3(impulse), true);
	}

	auto PhysicsWorld::GetLinearVelocity(const UUID entityId) const -> glm::vec3
	{
		b3BodyId body;
		if (!TryGetBody(entityId, body))
		{
			Log::Warn("Physics: GetLinearVelocity on entity {} which has no physics body.", static_cast<uint64_t>(entityId));
			return glm::vec3(0.0f);
		}

		return Utils::FromB3(b3Body_GetLinearVelocity(body));
	}

	auto PhysicsWorld::SetLinearVelocity(const UUID entityId, const glm::vec3& velocity) -> void
	{
		b3BodyId body;
		if (!TryGetBody(entityId, body))
		{
			Log::Warn("Physics: SetLinearVelocity on entity {} which has no physics body.", static_cast<uint64_t>(entityId));
			return;
		}

		b3Body_SetLinearVelocity(body, Utils::ToB3(velocity));
	}

	auto PhysicsWorld::CastRay(const glm::vec3& origin, const glm::vec3& direction, const float maxDistance) const -> RayHit
	{
		RayHit result;
		const float length = glm::length(direction);
		if (maxDistance <= 0.0f || length < 1e-6f)
			return result;

		const glm::vec3 translation = (direction / length) * maxDistance;
		const b3QueryFilter filter = b3DefaultQueryFilter();
		const b3RayResult ray = b3World_CastRayClosest(m_WorldId, Utils::ToB3(origin), Utils::ToB3(translation), filter);
		if (!ray.hit)
			return result;

		result.Hit = true;
		result.Point = Utils::FromB3(ray.point);
		result.Normal = Utils::FromB3(ray.normal);
		result.Distance = ray.fraction * maxDistance;

		const b3BodyId body = b3Shape_GetBody(ray.shapeId);
		if (B3_IS_NON_NULL(body))
			result.EntityId = UUID(reinterpret_cast<uintptr_t>(b3Body_GetUserData(body)));

		return result;
	}

	auto PhysicsWorld::OverlapsSphere(const UUID entityId, const glm::vec3& center, const float radius) const -> bool
	{
		struct Context
		{
			uint64_t TargetId;
			bool Found;
		} context{ static_cast<uint64_t>(entityId), false };

		const b3Vec3 point{ 0.0f, 0.0f, 0.0f };
		b3ShapeProxy proxy{};
		proxy.points = &point;
		proxy.count = 1;
		proxy.radius = radius;

		const b3QueryFilter filter = b3DefaultQueryFilter();
		b3World_OverlapShape(m_WorldId, Utils::ToB3(center), &proxy, filter, [](const b3ShapeId shapeId, void* ctx) -> bool
		{
			auto* c = static_cast<Context*>(ctx);
			const b3BodyId body = b3Shape_GetBody(shapeId);
			void* userData = B3_IS_NON_NULL(body) ? b3Body_GetUserData(body) : nullptr;
			if (userData && static_cast<uint64_t>(reinterpret_cast<uintptr_t>(userData)) == c->TargetId)
			{
				c->Found = true;
				return false; // stop traversal once the target body is found
			}
			return true;
		}, &context);

		return context.Found;
	}
}
