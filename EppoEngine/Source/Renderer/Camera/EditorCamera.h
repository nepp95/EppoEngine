#pragma once

#include "Event/MouseEvent.h"
#include "Renderer/Camera/Camera.h"

namespace Eppo
{
    class EditorCamera : public Camera
    {
    public:
        EditorCamera();
        EditorCamera(const glm::vec3& position, float pitch, float yaw);

        auto OnUpdate(float timestep) -> void;
        auto OnEvent(Event& e) -> void;

        auto SetViewportSize(const glm::vec2& size) -> void;
        auto SetViewportSize(uint32_t width, uint32_t height) -> void;
        auto SetViewportSize(float width, float height) -> void;

        [[nodiscard]] auto GetPosition() const -> const glm::vec3& { return m_Position; }
        [[nodiscard]] auto GetPitch() const -> float { return m_Pitch; }
        [[nodiscard]] auto GetYaw() const -> float { return m_Yaw; }
        [[nodiscard]] auto GetNearClip() const -> float { return m_NearClip; }
        [[nodiscard]] auto GetFarClip() const -> float { return m_FarClip; }

        [[nodiscard]] auto GetViewMatrix() const -> const glm::mat4& { return m_View; }
        [[nodiscard]] auto GetViewProjection() const -> glm::mat4 { return m_Projection * m_View; }

    private:
        auto UpdateCameraVectors() -> void;

    private:
        glm::mat4 m_View{};

        glm::vec3 m_Position{};
        glm::vec3 m_FrontDirection = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 m_UpDirection{};
        glm::vec3 m_RightDirection{};
        glm::vec3 m_WorldUpDirection = glm::vec3(0.0f, 1.0f, 0.0f);

        float m_Pitch = 0.0f;
        float m_Yaw = 0.0f;
        float m_NearClip = 0.1f;
        float m_FarClip = 100.0f;

        float m_Zoom = 45.0f;
        float m_MovementSpeed = 3.0f;

        glm::vec2 m_ViewportSize = glm::vec2(1280.0f, 720.0f);
    };
}