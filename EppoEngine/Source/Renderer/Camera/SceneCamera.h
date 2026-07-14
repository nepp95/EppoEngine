#pragma once

#include "Renderer/Camera/Camera.h"

namespace Eppo
{
	class SceneCamera : public Camera
	{
	public:
		SceneCamera();

		auto SetPerspective(float verticalFov, float nearClip, float farClip) -> void;
		auto SetViewportSize(uint32_t width, uint32_t height) -> void;

		[[nodiscard]] auto GetPerspectiveVerticalFov() const -> float { return m_VerticalFov; }
		auto SetPerspectiveVerticalFov(float verticalFov) -> void { m_VerticalFov = verticalFov; RecalculateProjection(); }

		[[nodiscard]] auto GetPerspectiveNearClip() const -> float { return m_NearClip; }
		auto SetPerspectiveNearClip(float nearClip) -> void { m_NearClip = nearClip; RecalculateProjection(); }

		[[nodiscard]] auto GetPerspectiveFarClip() const -> float { return m_FarClip; }
		auto SetPerspectiveFarClip(float farClip) -> void { m_FarClip = farClip; RecalculateProjection(); }

	private:
		auto RecalculateProjection() -> void;

	private:
		float m_VerticalFov = 45.0f; // degrees
		float m_NearClip = 0.1f;
		float m_FarClip = 1000.0f;
		float m_AspectRatio = 16.0f / 9.0f;
	};
}
