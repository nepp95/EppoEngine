#pragma once

#include "Renderer/Camera/Camera.h"

namespace Eppo
{
	enum class ProjectionType : uint8_t
	{
		Orthographic,
		Perspective
	};

	class SceneCamera : public Camera
	{
	public:
		SceneCamera();
		~SceneCamera() override = default;

		void SetPerspective(float fov, float nearClip, float farClip);
		void SetOrthographic(float size, float nearClip, float farClip);

		void SetViewportSize(uint32_t width, uint32_t height);
		void SetViewportSize(float width, float height);

		[[nodiscard]] float GetPerspectiveFov() const { return m_PerspectiveFov; }
		void SetPerspectiveFov(float fov);

		[[nodiscard]] float GetPerspectiveNearClip() const { return m_PerspectiveNearClip; }
		void SetPerspectiveNearClip(float nearClip);

		[[nodiscard]] float GetPerspectiveFarClip() const { return m_PerspectiveFarClip; }
		void SetPerspectiveFarClip(float farClip);

		[[nodiscard]] float GetOrthographicSize() const { return m_OrthographicSize; }
		void SetOrthographicSize(float size);

		[[nodiscard]] float GetOrthographicNearClip() const { return m_OrthographicNearClip; }
		void SetOrthographicNearClip(float nearClip);

		[[nodiscard]] float GetOrthographicFarClip() const { return m_OrthographicFarClip; }
		void SetOrthographicFarClip(float farClip);

		[[nodiscard]] ProjectionType GetProjectionType() const { return m_ProjectionType; }
		void SetProjectionType(ProjectionType type);

	private:
		void RecalculateProjection();

	private:
		ProjectionType m_ProjectionType = ProjectionType::Orthographic;

		float m_PerspectiveFov = glm::radians(45.0f);
		float m_PerspectiveNearClip = 0.01f;
		float m_PerspectiveFarClip = 1000.0f;

		float m_OrthographicSize = 10.0f;
		float m_OrthographicNearClip = -1.0f;
		float m_OrthographicFarClip = 1.0f;

		float m_AspectRatio = 0.0f;
	};
}
