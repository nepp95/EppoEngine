#include "pch.h"
#include "Camera.h"

#include "Core/Input.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Eppo
{
	Camera::Camera(float fieldOfView, float aspectRatio, float nearClip, float farClip)
		: m_Fov(fieldOfView), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip)
	{
		m_ProjectionMatrix = glm::perspective(glm::radians(m_Fov), m_AspectRatio, m_NearClip, m_FarClip);
		UpdateCameraView();
	}

	void Camera::OnUpdate(float timestep)
	{
		EPPO_PROFILE_FN("CPU Update", "Camera Update");

		float cameraSpeed = timestep;

		if (Input::IsKeyPressed(Key::LeftAlt))
		{
			glm::vec2 mousePosition = Input::GetMousePosition();
			glm::vec2 delta = (mousePosition - m_MousePosition) * 0.003f;
			m_MousePosition = mousePosition;

			if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle))
				MousePan(delta);
			else if (Input::IsMouseButtonPressed(Mouse::ButtonLeft))
				MouseRotate(delta);
			else if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
				MouseZoom(delta.y);
		}
		
		UpdateCameraView();
	}

	void Camera::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(Camera::OnMouseScroll));
	}

	void Camera::SetViewportSize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		m_AspectRatio = (float)m_ViewportWidth / (float)m_ViewportHeight;
		m_ProjectionMatrix = glm::perspective(glm::radians(m_Fov), m_AspectRatio, m_NearClip, m_FarClip);
	}

	void Camera::SetDistance(float distance)
	{
		m_Distance = distance;
	}

	bool Camera::OnMouseScroll(MouseScrolledEvent& e)
	{
		float delta = e.GetYOffset() * 0.1f;
		MouseZoom(delta);
		UpdateCameraView();

		return false;
	}

	void Camera::UpdateCameraView()
	{
		m_Position = CalculatePosition();

		glm::quat orientation = GetOrientation();
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}

	void Camera::MousePan(const glm::vec2& delta)
	{
		float xSpeed, ySpeed, x, y;

		x = std::min(m_ViewportWidth / 1000.0f, 2.4f);
		xSpeed = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

		y = std::min(m_ViewportHeight / 1000.0f, 2.4f);
		ySpeed = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

		m_FocalPoint += GetRightDirection() * delta.x * xSpeed * m_Distance;
		m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
	}

	void Camera::MouseRotate(const glm::vec2& delta)
	{
		float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
		m_Yaw += yawSign * delta.x * m_RotationSpeed;
	}

	void Camera::MouseZoom(float delta)
	{
		float distance = m_Distance * m_ZoomSpeed;
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

	glm::vec3 Camera::GetUpDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::vec3 Camera::GetRightDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	glm::vec3 Camera::GetForwardDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}

	glm::quat Camera::GetOrientation() const
	{
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}

	glm::vec3 Camera::CalculatePosition() const
	{
		return m_FocalPoint - GetForwardDirection() * m_Distance;
	}

	ActualCamera::ActualCamera()
	{
		m_ProjectionMatrix = glm::perspective(glm::radians(30.0f), 1.778f, 0.1f, 1000.0f);
		UpdateCameraVectors();
	}

	void ActualCamera::OnUpdate(float timestep)
	{
		float cameraSpeed = timestep * 2.0f;

		if (timeTest >= 1.0f)
		{
			EPPO_INFO("Camera position: {}", m_Position);
			EPPO_INFO("Camera direction: {}", m_Direction);
			timeTest = 0.0f;
		}
		timeTest += timestep;

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

	void ActualCamera::OnEvent(Event& e)
	{

	}

	void ActualCamera::UpdateCameraVectors()
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
