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
		TagComponent(const std::string& tag)
			: Tag(tag)
		{}
	};

	struct TransformComponent
	{
		glm::vec3 Translation = glm::vec3(0.0f);
		glm::vec3 Rotation = glm::vec3(0.0f);
		glm::vec3 Scale = glm::vec3(1.0f);

		TransformComponent() = default;
		TransformComponent(const glm::vec3& translation)
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
		MeshComponent(const Ref<Mesh>& mesh)
			: MeshHandle(mesh->Handle)
		{}

		MeshComponent(const AssetHandle handle)
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

	// Attaches a user script class to an entity. Kept intentionally small: it
	// only names the class. The per-instance field values live in a side table
	// owned by ScriptEngine (keyed by entity UUID), so this component stays
	// cheap to store and copy in the registry.
	struct ScriptComponent
	{
		std::string ClassName;

		ScriptComponent() = default;
		ScriptComponent(std::string className)
			: ClassName(std::move(className))
		{}
	};
}