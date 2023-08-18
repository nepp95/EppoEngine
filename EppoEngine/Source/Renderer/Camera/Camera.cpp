#include "pch.h"
#include "Camera.h"

#include "Core/Input.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Eppo
{
	Camera::Camera(float fieldOfView, float aspectRatio, float nearClip, float farClip)
		: m_Fov(fieldOfView), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip)
	{
		m_Direction = glm::normalize(m_Position - m_FocalPoint);
		m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Front, m_Up);
		m_ProjectionMatrix = glm::perspective(glm::radians(m_Fov), m_AspectRatio, m_NearClip, m_FarClip);
	}

	void Camera::OnUpdate(float timestep)
	{
		float cameraSpeed = timestep;

		if (Input::IsKeyPressed(Key::W))
			m_Position += m_Front * cameraSpeed;
		if (Input::IsKeyPressed(Key::S))
			m_Position -= m_Front * cameraSpeed;
		if (Input::IsKeyPressed(Key::A))
			m_Position -= glm::normalize(glm::cross(m_Front, m_Up)) * cameraSpeed;
		if (Input::IsKeyPressed(Key::D))
			m_Position += glm::normalize(glm::cross(m_Front, m_Up)) * cameraSpeed;

		m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Front, m_Up);

		EPPO_TRACE("Position: {}", m_Position);
	}

	glm::vec3 Camera::GetRightDirection()
	{
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 right = glm::normalize(glm::cross(up, m_Direction));

		return right;
	}

	glm::vec3 Camera::GetUpDirection()
	{
		glm::vec3 up = glm::cross(m_Direction, GetRightDirection());

		return up;
	}
}
