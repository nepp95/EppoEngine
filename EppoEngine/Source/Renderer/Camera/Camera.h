#pragma once

#include <glm/glm.hpp>

namespace Eppo
{
	class Camera
	{
	public:
		Camera() = default;
		Camera(const glm::mat4& projectionMatrix)
			: m_ProjectionMatrix(projectionMatrix)
		{}

		virtual ~Camera() = default;

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }

	protected:
		glm::mat4 m_ProjectionMatrix;
	};
}
