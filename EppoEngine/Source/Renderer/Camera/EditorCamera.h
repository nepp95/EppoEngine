#pragma once

#include "Event/MouseEvent.h"
#include "Renderer/Camera/Camera.h"

namespace Eppo
{
	class EditorCamera : public Camera
	{
	public:
		EditorCamera();
		EditorCamera(const glm::vec3& position);
		~EditorCamera() = default;

		void OnUpdate(float timestep);
		void OnEvent(Event& e);

		void SetViewportSize(const glm::vec2& size);

		void SetPosition(const glm::vec3& position);
		glm::vec3 GetPosition() const { return m_Position; }

		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjectionMatrix() const { return m_ProjectionMatrix * m_ViewMatrix; }

	private:
		void UpdateCameraVectors();

	private:
		glm::mat4 m_ViewMatrix;

		glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 m_FrontDirection = glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 m_UpDirection;
		glm::vec3 m_RightDirection;
		glm::vec3 m_WorldUpDirection = glm::vec3(0.0f, 1.0f, 0.0f);

		float m_Pitch = 0.0f;
		float m_Yaw = 90.0f;

		float m_MovementSpeed = 3.0f;
		float m_MouseSensitivity = 0.1f;
		float m_Zoom = 45.0f;
		
		glm::vec2 m_ViewportSize = { 1280.0f, 720.0f };
	};
}
