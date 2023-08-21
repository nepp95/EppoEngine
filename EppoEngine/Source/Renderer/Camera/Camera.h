#pragma once

#include "Events/MouseEvent.h"

#include <glm/glm.hpp>

namespace Eppo
{
	class Camera
	{
	public:
		Camera(float fieldOfView, float aspectRatio, float nearClip = 0.1f, float farClip = 1000.0f);
		~Camera() = default;

		void OnUpdate(float timestep);
		void OnEvent(Event& e);

		void SetViewportSize(float width, float height);

		float GetDistance() const { return m_Distance; }
		void SetDistance(float distance);

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjectionMatrix() const { return m_ProjectionMatrix * m_ViewMatrix; }

		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }
		float GetRoll() const { return m_Roll; }

		const glm::vec3& GetPosition() const { return m_Position; }

	private:
		bool OnMouseScroll(MouseScrolledEvent& e);

		void UpdateCameraView();

		void MousePan(const glm::vec2& delta);
		void MouseRotate(const glm::vec2& delta);
		void MouseZoom(float delta);

		glm::vec3 GetUpDirection() const;
		glm::vec3 GetRightDirection() const;
		glm::vec3 GetForwardDirection() const;

		glm::quat GetOrientation() const;

		glm::vec3 CalculatePosition() const;

	private:
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ProjectionMatrix;

		glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 3.0f); // TODO: Change to scene origin (0.0f)
		glm::vec3 m_FocalPoint = glm::vec3(0.0f);

		float m_Pitch = 0.0f; // up/down
		float m_Yaw = 0.0f; // left/right
		float m_Roll = 0.0f;

		float m_Fov = 45.0f;
		float m_AspectRatio = 1.778f;
		float m_NearClip = 0.1f;
		float m_FarClip = 1000.0f;
		float m_Distance = 10.0f;

		float m_ZoomSpeed = 0.2f;
		float m_RotationSpeed = 0.8f;

		glm::vec2 m_MousePosition = glm::vec2(0.0f);

		uint32_t m_ViewportWidth = 1280;
		uint32_t m_ViewportHeight = 720;
	};
}
