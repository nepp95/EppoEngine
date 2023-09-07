#pragma once

#include "Asset/Asset.h"
#include "Core/UUID.h"

#include <glm/glm.hpp>
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
		std::string Texture;
		glm::vec4 Color = glm::vec4(1.0f);

		SpriteComponent() = default;
		SpriteComponent(const SpriteComponent&) = default;
		SpriteComponent(const glm::vec4& color)
			: Color(color)
		{}
	};

	struct ColorComponent
	{
		glm::vec4 Color = glm::vec4(1.0f);

		ColorComponent() = default;
		ColorComponent(const ColorComponent&) = default;
		ColorComponent(const glm::vec4& color)
			: Color(color)
		{}

		operator glm::vec4() { return Color; }
		operator const glm::vec4&() const { return Color; }
	};
}
