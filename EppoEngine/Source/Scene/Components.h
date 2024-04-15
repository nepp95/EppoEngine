#pragma once

#include "Asset/Asset.h"
#include "Core/UUID.h"
#include "Physics/RigidBody.h"

#include <glm/glm.hpp>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/ext/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Eppo
{
	struct IDComponent
	{
		UUID ID;

		IDComponent() = default;
		IDComponent(const IDComponent& other) = default;

		operator UUID&() { return ID; }
		operator const UUID&() const { return ID; }
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent& other) = default;
		TagComponent(const std::string& tag)
			: Tag(tag)
		{}

		operator std::string&() { return Tag; }
		operator const std::string&() const { return Tag; }
	};

	struct TransformComponent
	{
		glm::vec3 Translation = glm::vec3(0.0f);
		glm::vec3 Rotation = glm::vec3(0.0f);
		glm::vec3 Scale = glm::vec3(1.0f);

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& translation)
			: Translation(translation)
		{}

		glm::mat4 GetTransform() const
		{
			return glm::translate(glm::mat4(1.0f), Translation)
				* glm::toMat4(glm::quat(Rotation))
				* glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	struct SpriteComponent
	{
		AssetHandle TextureHandle = 0;
		glm::vec4 Color = glm::vec4(1.0f);

		SpriteComponent() = default;
		SpriteComponent(const SpriteComponent&) = default;
		SpriteComponent(const glm::vec4& color)
			: Color(color)
		{}
	};

	struct MeshComponent
	{
		AssetHandle MeshHandle = 0;

		MeshComponent() = default;
		MeshComponent(const MeshComponent&) = default;
	};

	struct DirectionalLightComponent
	{
		glm::vec3 Direction;
		glm::vec4 AlbedoColor = glm::vec4(1.0f);
		glm::vec4 AmbientColor = glm::vec4(1.0f);
		glm::vec4 SpecularColor = glm::vec4(1.0f);

		DirectionalLightComponent() = default;
		DirectionalLightComponent(const DirectionalLightComponent&) = default;
	};

	struct ScriptComponent
	{
		std::string ClassName;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent&) = default;
	};
	
	struct RigidBodyComponent
	{
		enum class BodyType { Static, Dynamic, Kinematic };
		BodyType Type = BodyType::Static;

		float Mass = 1.0f;

		RigidBody RuntimeBody;

		RigidBodyComponent() = default;
		RigidBodyComponent(const RigidBodyComponent&) = default;
	};
}
