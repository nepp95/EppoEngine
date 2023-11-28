#include "pch.h"
#include "EditorCamera.h"

#include "Core/Input.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Eppo
{
	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
		: m_Fov(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip)
	{
		m_ProjectionMatrix = glm::perspective(glm::radians(m_Fov), m_AspectRatio, m_NearClip, m_FarClip);
		UpdateCameraVectors();
	}

	void EditorCamera::OnUpdate(float timestep)
	{
		if (Input::IsKeyPressed(Key::LeftAlt))
		{
			glm::vec2 mousePosition = Input::GetMousePosition();
			glm::vec2 delta = (mousePosition - m_MousePosition) * 0.01f;
			m_MousePosition = mousePosition;

			if (Input::IsMouseButtonPressed(Mouse::ButtonLeft))
				MouseRotate(delta);
			else if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
				MouseZoom(delta.y);
			else if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle))
				MousePan(delta);
		}

		UpdateCameraVectors();
	}

	void EditorCamera::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(EditorCamera::OnMouseScroll));
	}

	void EditorCamera::SetViewportSize(const glm::vec2& size)
	{
		m_ViewportSize = size;

		// Update projection matrix
		m_AspectRatio = m_ViewportSize.x / m_ViewportSize.y;
		m_ProjectionMatrix = glm::perspective(glm::radians(m_Fov), m_AspectRatio, m_NearClip, m_FarClip);
	}

	void EditorCamera::SetDistance(float distance)
	{
		m_Distance = distance;
		UpdateCameraVectors();
	}

	bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e)
	{
		float delta = e.GetYOffset() * 0.1f;
		MouseZoom(delta);
		UpdateCameraVectors();

		return false;
	}

	void EditorCamera::UpdateCameraVectors()
	{
		m_Position = CalculatePosition();
		glm::quat orientation = CalculateOrientation();

		m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}

	void EditorCamera::MouseRotate(const glm::vec2& delta)
	{
		float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
		m_Yaw += yawSign * delta.x * m_RotationSpeed;
	}

	void EditorCamera::MousePan(const glm::vec2& delta)
	{
		float xSpeed, ySpeed;
		float x, y;

		x = std::min(m_ViewportSize.x / 1000.0f, 2.4f);
		xSpeed = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

		y = std::min(m_ViewportSize.y / 1000.0f, 2.4f);
		ySpeed = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

		m_FocalPoint += GetRightDirection() * delta.x * xSpeed * m_Distance;
		m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
	}

	void EditorCamera::MouseZoom(float delta)
	{
		float distance = m_Distance * 0.2f;
		distance = std::max(distance, 0.0f);

		float speed = distance * distance;
		speed = std::min(speed, 100.0f);

		m_Distance -= delta * speed;

		if (m_Distance < 1.0f)
		{
			m_FocalPoint += GetForwardDirection();
			m_Distance = 1.0f;
		}
	}

	glm::vec3 EditorCamera::CalculatePosition() const
	{
		return m_FocalPoint - GetForwardDirection() * m_Distance;
	}

	glm::quat EditorCamera::CalculateOrientation() const
	{
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}

	glm::vec3 EditorCamera::GetForwardDirection() const
	{
		return glm::rotate(CalculateOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}

	glm::vec3 EditorCamera::GetUpDirection() const
	{
		return glm::rotate(CalculateOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::vec3 EditorCamera::GetRightDirection() const
	{
		return glm::rotate(CalculateOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}
}
