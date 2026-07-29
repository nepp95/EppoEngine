#pragma once

#include <EppoEngine.h>

#include <string>

namespace Eppo
{
    class RuntimeLayer final : public Layer
    {
    public:
        // The package is read before the application exists, because its shaders are needed during startup.
        explicit RuntimeLayer(GameData gameData)
            : m_GameData(std::move(gameData))
        {}

        auto OnAttach() -> void override;
        auto OnDetach() -> void override;

        auto OnUpdate(float timestep) -> void override;

#if !defined(EP_DIST)
        auto OnUIRender() -> void override;
#endif

        auto OnEvent(Event& event) -> void override;

    private:
        auto OnWindowResize(const WindowResizeEvent& event) const -> bool;
        auto OnKeyPressed(const KeyPressedEvent& event) -> bool;
        auto Resize(uint32_t width, uint32_t height) const -> void;
        auto Fail(const std::string& message) -> void;

    private:
        GameData m_GameData;

        Ref<AssetManager> m_AssetManager = nullptr;
        Ref<Project> m_Project = nullptr;
        Ref<Scene> m_Scene = nullptr;
        Ref<SceneRenderer> m_SceneRenderer = nullptr;

        bool m_ScriptEngineInitialized = false;
        bool m_RuntimeStarted = false;
        bool m_StartupComplete = false;

#if !defined(EP_DIST)
        bool m_ShowOverlay = false;
#endif
    };
}
