#include "pch.h"
#include "Renderer/Camera/EditorCamera.h"

#include "Core/Input.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Eppo
{
    EditorCamera::EditorCamera()
    {
        UpdateCameraVectors();
    }

    EditorCamera::EditorCamera(const glm::vec3& position, float pitch, float yaw)
        : m_Position(position), m_Pitch(pitch), m_Yaw(yaw)
    {
        UpdateCameraVectors();
    }

    auto EditorCamera::OnUpdate(const float timestep) -> void
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

        if (Input::IsKeyPressed(Key::R))
            m_Pitch += velocity * 2.0f;
        if (Input::IsKeyPressed(Key::F))
            m_Pitch -= velocity * 2.0f;

        UpdateCameraVectors();
    }

    auto EditorCamera::OnEvent(Event& e) -> void {}

    auto EditorCamera::SetViewportSize(const glm::vec2& size) -> void
    {
        m_ViewportSize = size;
    }

    auto EditorCamera::SetViewportSize(const uint32_t width, const uint32_t height) -> void
    {
        SetViewportSize({ static_cast<float>(width), static_cast<float>(height) });
    }

    auto EditorCamera::SetViewportSize(float width, float height) -> void
    {
        SetViewportSize({ width, height });
    }

    auto EditorCamera::UpdateCameraVectors() -> void
    {
        const auto front = glm::vec3(
            cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch)), sin(glm::radians(m_Pitch)),
            sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch))
        );

        m_FrontDirection = glm::normalize(front);
        m_RightDirection = glm::normalize(glm::cross(m_FrontDirection, m_WorldUpDirection));
        m_UpDirection = glm::normalize(glm::cross(m_RightDirection, m_FrontDirection));

        m_View = glm::lookAt(m_Position, m_Position + m_FrontDirection, m_UpDirection);
        m_Projection = glm::perspective(glm::radians(m_Zoom), m_ViewportSize.x / m_ViewportSize.y, m_NearClip, m_FarClip);
    }
}