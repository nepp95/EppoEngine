#include "pch.h"
#include "EditorCamera.h"

#include "Core/Input.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Eppo
{
	EditorCamera::EditorCamera()
	{
		UpdateCameraVectors();
	}

	void EditorCamera::OnUpdate(float timestep)
	{
		float velocity = m_MovementSpeed * timestep;

		if (Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift))
			velocity *= 3.0f;

		if (Input::IsKeyPressed(Key::W))
			m_Position += m_FrontDirection * velocity;
		if (Input::IsKeyPressed(Key::S))
			m_Position -= m_FrontDirection * velocity;
		if (Input::IsKeyPressed(Key::A))
			m_Position -= m_RightDirection * velocity;
		if (Input::IsKeyPressed(Key::D))
			m_Position += m_RightDirection * velocity;

		if (Input::IsKeyPressed(Key::Q))
			m_Yaw -= velocity * 2.0f;
		if (Input::IsKeyPressed(Key::E))
			m_Yaw += velocity * 2.0f;

		UpdateCameraVectors();
	}

	void EditorCamera::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		//dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(EditorCamera::OnMouseScroll));
	}

	void EditorCamera::SetViewportSize(const glm::vec2& size)
	{
		if (m_ViewportSize == size)
			return;

		m_ViewportSize = size;
	}

	void EditorCamera::UpdateCameraVectors()
	{
		glm::vec3 front = glm::vec3(
			cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch)),
			sin(glm::radians(m_Pitch)),
			sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch))
		);

		m_FrontDirection = glm::normalize(front);
		m_RightDirection = glm::normalize(glm::cross(m_FrontDirection, m_WorldUpDirection));
		m_UpDirection = glm::normalize(glm::cross(m_RightDirection, m_FrontDirection));

		m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_FrontDirection, m_UpDirection);
		m_ProjectionMatrix = glm::perspective(glm::radians(m_Zoom), m_ViewportSize.x / m_ViewportSize.y, 0.1f, 1000.0f);
	}
}
