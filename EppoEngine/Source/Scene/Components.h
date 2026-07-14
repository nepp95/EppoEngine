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

	struct CameraComponent
	{
		SceneCamera Camera;
		bool Primary = true;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	struct PointLightComponent
	{
		glm::vec3 Color = glm::vec3(1.0f);
		float Intensity = 10.0f;

		PointLightComponent() = default;
		PointLightComponent(const PointLightComponent&) = default;
	};

	struct ScriptComponent
	{
		std::string ClassName;

		ScriptComponent() = default;
        explicit ScriptComponent(std::string className)
			: ClassName(std::move(className))
		{}
	};

	struct RelationshipComponent
	{
		UUID Parent = 0;
		std::vector<UUID> Children;

		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent&) = default;
	};

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

	struct BoxColliderComponent
	{
        glm::vec3 HalfSize = glm::vec3(0.5f);
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