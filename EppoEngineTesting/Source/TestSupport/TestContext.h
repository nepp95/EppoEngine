#pragma once

#include "Core/Base.h"
#include "Scene/Scene.h"

#include <functional>

namespace Eppo::Testing
{
    // Scenario façade: boots the shared AppHarness and owns a fresh Scene.
    // AdvanceFrames runs per-frame logic through real frames.
    class TestContext
    {
    public:
        TestContext();
        ~TestContext();

        TestContext(const TestContext&) = delete;
        TestContext& operator=(const TestContext&) = delete;

        // Whether the graphical app booted (needs display+GPU); scenarios early-return if not.
        [[nodiscard]] auto IsAvailable() const -> bool;

        [[nodiscard]] auto GetScene() -> const Ref<Scene>& { return m_Scene; }

        // Advance `count` real frames, running `perFrame` before each step (no-op if unavailable).
        auto AdvanceFrames(uint32_t count, const std::function<void(float)>& perFrame = {}, float timestep = 1.0f / 60.0f) -> void;

    private:
        Ref<Scene> m_Scene;
    };
}
