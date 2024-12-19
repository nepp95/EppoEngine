#include "pch.h"
#include "SceneCamera.h"

namespace Eppo
{
	SceneCamera::SceneCamera()
	{
		RecalculateProjection();
	}

	void SceneCamera::SetPerspective(const float fov, const float nearClip, const float farClip)
	{
		EPPO_PROFILE_FUNCTION("SceneCamera::SetPerspective");

		m_ProjectionType = ProjectionType::Perspective;

		m_PerspectiveFov = fov;
		m_PerspectiveNearClip = nearClip;
		m_PerspectiveFarClip = farClip;

		RecalculateProjection();
	}

	void SceneCamera::SetOrthographic(const float size, const float nearClip, const float farClip)
	{
		EPPO_PROFILE_FUNCTION("SceneCamera::SetOrthographic");

		m_ProjectionType = ProjectionType::Orthographic;

		m_OrthographicSize = size;
		m_OrthographicNearClip = nearClip;
		m_OrthographicFarClip = farClip;

		RecalculateProjection();
	}

	void SceneCamera::SetViewportSize(const uint32_t width, const uint32_t height)
	{
		EPPO_PROFILE_FUNCTION("SceneCamera::SetViewportSize");

		SetViewportSize(static_cast<float>(width), static_cast<float>(height));
	}

	void SceneCamera::SetViewportSize(const float width, const float height)
	{
		EPPO_PROFILE_FUNCTION("SceneCamera::SetViewportSize");
		EPPO_ASSERT(width > 0 && height > 0)

		m_AspectRatio = width / height;
		RecalculateProjection();
	}

	void SceneCamera::SetPerspectiveFov(const float fov)
	{
		EPPO_PROFILE_FUNCTION("SceneCamera::SetPerspectiveFov");

		m_PerspectiveFov = fov;
		RecalculateProjection();
	}

	void SceneCamera::SetPerspectiveNearClip(const float nearClip)
	{
		EPPO_PROFILE_FUNCTION("SceneCamera::SetPerspectiveNearClip");

		m_PerspectiveNearClip = nearClip;
		RecalculateProjection();
	}

	void SceneCamera::SetPerspectiveFarClip(const float farClip)
	{
		EPPO_PROFILE_FUNCTION("SceneCamera::SetPerspectiveFarClip");

		m_PerspectiveFarClip = farClip;
		RecalculateProjection();
	}

	void SceneCamera::SetOrthographicSize(const float size)
	{
		EPPO_PROFILE_FUNCTION("SceneCamera::SetOrthographicSize");

		m_OrthographicSize = size;
		RecalculateProjection();
	}

	void SceneCamera::SetOrthographicNearClip(const float nearClip)
	{
		EPPO_PROFILE_FUNCTION("SceneCamera::SetOrthographicNearClip");

		m_OrthographicNearClip = nearClip;
		RecalculateProjection();
	}

	void SceneCamera::SetOrthographicFarClip(const float farClip)
	{
		EPPO_PROFILE_FUNCTION("SceneCamera::SetOrthographicFarClip");

		m_OrthographicFarClip = farClip;
		RecalculateProjection();
	}

	void SceneCamera::SetProjectionType(const ProjectionType type)
	{
		EPPO_PROFILE_FUNCTION("SceneCamera::SetProjectionType");

		m_ProjectionType = type;
		RecalculateProjection();
	}

	void SceneCamera::RecalculateProjection()
	{
		EPPO_PROFILE_FUNCTION("SceneCamera::RecalculateProjection");

		if (m_ProjectionType == ProjectionType::Perspective)
		{
			m_ProjectionMatrix = glm::perspective(m_PerspectiveFov, m_AspectRatio, m_PerspectiveNearClip, m_PerspectiveFarClip);
		} else if (m_ProjectionType == ProjectionType::Orthographic)
		{
			const float left = -m_OrthographicSize * m_AspectRatio * 0.5f;
			const float right = m_OrthographicSize * m_AspectRatio * 0.5f;
			const float bottom = -m_OrthographicSize * 0.5f;
			const float top = m_OrthographicSize * 0.5f;

			m_ProjectionMatrix = glm::ortho(left, right, bottom, top, m_OrthographicNearClip, m_OrthographicFarClip);
		}
	}
}
