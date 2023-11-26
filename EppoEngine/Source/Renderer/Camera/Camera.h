#pragma once

#include "Event/MouseEvent.h"

#include <glm/glm.hpp>

namespace Eppo
{
	class Camera
	{
	public:
		Camera();
		~Camera() = default;

		void OnUpdate(float timestep);
		void OnEvent(Event& e);

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjectionMatrix() const { return m_ProjectionMatrix * m_ViewMatrix; }

	private:
		void UpdateCameraVectors();

	private:
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
		
		glm::vec3 m_Position = { 0.0f, 0.0f, 3.0f };
		glm::vec3 m_Direction = { 0.0f, 0.0f, 0.0f };

		glm::vec3 m_FrontDirection = { 0.0f, 0.0f, -1.0f };
		glm::vec3 m_UpDirection = { 0.0f, 1.0f, 0.0f };

		float m_Yaw = -90.0f;
		float m_Pitch = 0.0f;

		glm::vec2 m_MousePosition = { 0.0f, 0.0f };

		float timeTest = 0.0f;
	};
}
