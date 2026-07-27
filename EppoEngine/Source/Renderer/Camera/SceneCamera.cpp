#include "pch.h"
#include "Renderer/Camera/SceneCamera.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

namespace Eppo
{
    SceneCamera::SceneCamera()
    {
        RecalculateProjection();
    }

    auto SceneCamera::SetPerspective(float verticalFov, float nearClip, float farClip) -> void
    {
        m_VerticalFov = verticalFov;
        m_NearClip = nearClip;
        m_FarClip = farClip;
        RecalculateProjection();
    }

    auto SceneCamera::SetViewportSize(uint32_t width, uint32_t height) -> void
    {
        if (width == 0 || height == 0)
            return;

        m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
        RecalculateProjection();
    }

    auto SceneCamera::RecalculateProjection() -> void
    {
        m_Projection = glm::perspective(glm::radians(m_VerticalFov), m_AspectRatio, m_NearClip, m_FarClip);
    }
}
