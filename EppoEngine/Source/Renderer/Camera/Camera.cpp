#include "pch.h"
#include "Camera.h"

#include "Core/Input.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Eppo
{
	Camera::Camera()
	{
		m_ProjectionMatrix = glm::perspective(glm::radians(30.0f), 1.778f, 0.1f, 1000.0f);
		UpdateCameraVectors();
	}

	void Camera::OnUpdate(float timestep)
	{
		float cameraSpeed = timestep * 2.0f;

		if (Input::IsKeyPressed(Key::LeftAlt))
		{
			glm::vec2 mousePosition = Input::GetMousePosition();
			glm::vec2 delta = (mousePosition - m_MousePosition) * 0.003f;
			m_MousePosition = mousePosition;

			m_Yaw += delta.x;
			m_Pitch += delta.y;

			if (m_Pitch > 90.0f)
				m_Pitch = 90.0f;
			if (m_Pitch < -90.0f)
				m_Pitch = -90.0f;
		} else
		{
			if (Input::IsKeyPressed(Key::W))
				m_Position += m_FrontDirection * cameraSpeed;
			if (Input::IsKeyPressed(Key::S))
				m_Position -= m_FrontDirection * cameraSpeed;
			if (Input::IsKeyPressed(Key::A))
				m_Position -= glm::normalize(glm::cross(m_FrontDirection, m_UpDirection)) * cameraSpeed;
			if (Input::IsKeyPressed(Key::D))
				m_Position += glm::normalize(glm::cross(m_FrontDirection, m_UpDirection)) * cameraSpeed;
		}

		UpdateCameraVectors();
	}

	void Camera::OnEvent(Event& e)
	{

	}

	void Camera::UpdateCameraVectors()
	{
		m_Direction = {
			cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch)),
			sin(glm::radians(m_Pitch)),
			sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch))
		};

		m_FrontDirection = glm::normalize(m_Direction);

		m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_FrontDirection, m_UpDirection);
	}
}
