#pragma once

#include "Core/UUID.h"
#include "Renderer/Camera/SceneCamera.h"
#include "Renderer/Mesh.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Eppo
{
	struct IDComponent
	{
		UUID ID;

		IDComponent() = default;
		IDComponent(const UUID& id)
			: ID(id)
		{}
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
        explicit TagComponent(std::string tag)
			: Tag(std::move(tag))
		{}
	};

	struct TransformComponent
	{
		glm::vec3 Translation = glm::vec3(0.0f);
		glm::vec3 Rotation = glm::vec3(0.0f);
		glm::vec3 Scale = glm::vec3(1.0f);

		TransformComponent() = default;
        explicit TransformComponent(const glm::vec3& translation)
			: Translation(translation)
		{}

		[[nodiscard]] auto GetTransform() const -> glm::mat4
		{
			return glm::translate(glm::mat4(1.0f), Translation)
				* glm::mat4_cast(glm::quat(Rotation))
				* glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	struct MeshComponent
	{
		AssetHandle MeshHandle = 0;

		MeshComponent() = default;
        explicit MeshComponent(const Ref<Mesh>& mesh)
			: MeshHandle(mesh->Handle)
		{}

        explicit MeshComponent(const AssetHandle handle)
			: MeshHandle(handle)
		{}
	};

	// Turns an entity into a camera. On play, the scene renders through the
	// primary camera entity, using its TransformComponent for the view. Primary
	// selects which camera is used when several exist (first primary wins).
	struct CameraComponent
	{
		SceneCamera Camera;
		bool Primary = true;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	// Turns an entity into a point light. Position comes from the entity's
	// TransformComponent; the scene submits these to the SceneRenderer each
	// frame. Intensity scales the radiance before inverse-square attenuation.
	struct PointLightComponent
	{
		glm::vec3 Color = glm::vec3(1.0f);
		float Intensity = 10.0f;

		PointLightComponent() = default;
		PointLightComponent(const PointLightComponent&) = default;
	};

	// Attaches a user script class to an entity. Kept intentionally small: it
	// only names the class. The per-instance field values live in a side table
	// owned by ScriptEngine (keyed by entity UUID), so this component stays
	// cheap to store and copy in the registry.
	struct ScriptComponent
	{
		std::string ClassName;

		ScriptComponent() = default;
        explicit ScriptComponent(std::string className)
			: ClassName(std::move(className))
		{}
	};

	// Places an entity in the transform hierarchy. Links are stable UUIDs (not
	// handles) so they survive Scene::Copy and serialization. Parent == 0 is a root;
	// transforms are local, composed via Scene::GetWorldTransform. Every entity
	// carries one (added in CreateEntityWithUUID).
	struct RelationshipComponent
	{
		UUID Parent = 0;
		std::vector<UUID> Children;

		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent&) = default;
	};

	// Turns an entity into a physics body. On play the scene builds a Box3D body
	// from this + the entity's collider component(s); each frame it writes the
	// simulated pose back into the TransformComponent. Static never moves,
	// Kinematic is script/animation driven, Dynamic is fully simulated. No live
	// body handle is stored here — PhysicsWorld owns that (keyed by UUID), since
	// the scene is deep-copied on play and bodies are rebuilt at OnRuntimeStart.
	struct RigidBodyComponent
	{
		enum class BodyType : uint8_t { Static = 0, Kinematic, Dynamic };

		BodyType Type = BodyType::Static;
		float GravityScale = 1.0f;
		float LinearDamping = 0.0f;
		float AngularDamping = 0.0f;

		RigidBodyComponent() = default;
		RigidBodyComponent(const RigidBodyComponent&) = default;
	};

	// Box collider (a Box3D convex hull). HalfExtents/Offset are in the entity's
	// local space. Material (density/friction/restitution) lives on the collider
	// so different shapes on one body can differ.
	struct BoxColliderComponent
	{
		glm::vec3 HalfExtents = glm::vec3(0.5f);
		glm::vec3 Offset = glm::vec3(0.0f);
		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;

		BoxColliderComponent() = default;
		BoxColliderComponent(const BoxColliderComponent&) = default;
	};

	struct SphereColliderComponent
	{
		float Radius = 0.5f;
		glm::vec3 Offset = glm::vec3(0.0f);
		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;

		SphereColliderComponent() = default;
		SphereColliderComponent(const SphereColliderComponent&) = default;
	};

	// Capsule aligned to the local Y axis. Height is the distance between the two
	// hemisphere centers (the cylindrical part); total height is Height + 2*Radius.
	struct CapsuleColliderComponent
	{
		float Radius = 0.5f;
		float Height = 1.0f;
		glm::vec3 Offset = glm::vec3(0.0f);
		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;

		CapsuleColliderComponent() = default;
		CapsuleColliderComponent(const CapsuleColliderComponent&) = default;
	};
}