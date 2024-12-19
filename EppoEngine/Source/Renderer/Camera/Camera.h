#pragma once

#include <glm/glm.hpp>

namespace Eppo
{
	class Camera
	{
	public:
		Camera() = default;
		explicit Camera(const glm::mat4& projectionMatrix)
			: m_ProjectionMatrix(projectionMatrix)
		{}

		virtual ~Camera() = default;

		[[nodiscard]] const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }

	protected:
		glm::mat4 m_ProjectionMatrix;
	};
}
