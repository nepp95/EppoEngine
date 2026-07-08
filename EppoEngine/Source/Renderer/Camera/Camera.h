#pragma once

#include <glm/glm.hpp>

namespace Eppo
{
	class Camera
	{
	public:
		Camera() = default;
        explicit Camera(const glm::mat4& projection)
			: m_Projection(projection)
		{}

		virtual ~Camera() = default;

		[[nodiscard]] auto GetProjectionMatrix() const -> const glm::mat4& { return m_Projection; }

	protected:
		glm::mat4 m_Projection{};
	};
}