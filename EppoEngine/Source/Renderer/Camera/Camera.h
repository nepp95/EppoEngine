#pragma once

#include <glm/glm.hpp>

namespace Eppo
{
	class Camera
	{
	public:
		Camera(float fieldOfView, float aspectRatio, float nearClip = 0.1f, float farClip = 1000.0f);
		~Camera() = default;

		void OnUpdate(float timestep);

		glm::mat4 GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjectionMatrix() const { return m_ProjectionMatrix * m_ViewMatrix; }

	private:
		glm::vec3 GetRightDirection();
		glm::vec3 GetUpDirection();

	private:
		glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 3.0f);
		glm::vec3 m_Front = glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 m_FocalPoint = glm::vec3(0.0f);
		glm::vec3 m_Direction;

		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ProjectionMatrix;

		float m_Pitch = 0.0f; // rotate right/left
		float m_Yaw = 0.0f; // rotate up/down

		float m_Fov;
		float m_AspectRatio;

		float m_NearClip = 0.1f;
		float m_FarClip = 1000.0f;
	};
}
