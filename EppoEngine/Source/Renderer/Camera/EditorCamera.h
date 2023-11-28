#pragma once

#include "Event/MouseEvent.h"
#include "Renderer/Camera/Camera.h"

namespace Eppo
{
	class EditorCamera : public Camera
	{
	public:
		EditorCamera(float fov, float aspectRatio, float nearClip = 0.1f, float farClip = 1000.0f);
		~EditorCamera() = default;

		void OnUpdate(float timestep);
		void OnEvent(Event& e);

		void SetViewportSize(const glm::vec2& size);

		glm::vec3 GetPosition() const { return m_Position; }
		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }

		float GetDistance() const { return m_Distance; }
		void SetDistance(float distance);

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjectionMatrix() const { return m_ProjectionMatrix * m_ViewMatrix; }

	private:
		// Events
		bool OnMouseScroll(MouseScrolledEvent& e);

		// Update matrixes
		void UpdateCameraVectors();

		// Movement
		void MouseRotate(const glm::vec2& delta);
		void MousePan(const glm::vec2& delta);
		void MouseZoom(float delta);

		// Calculate position/orientation
		glm::vec3 CalculatePosition() const;
		glm::quat CalculateOrientation() const;

		// Directions
		glm::vec3 GetForwardDirection() const;
		glm::vec3 GetUpDirection() const;
		glm::vec3 GetRightDirection() const;

	private:
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);

		glm::vec3 m_Position = { 0.0f, 0.0f, 10.0f };
		glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };

		float m_Fov = 45.0f;
		float m_AspectRatio = 1.778f;
		float m_NearClip = 0.1f;
		float m_FarClip = 1000.0f;
		float m_Distance = 10.0f;

		float m_Pitch = 0.0f;
		float m_Yaw = 0.0f;
		
		float m_RotationSpeed = 0.8f;
		glm::vec2 m_MousePosition = { 0.0f, 0.0f };

		glm::vec2 m_ViewportSize = { 1280.0f, 720.0f };
	};
}
