#pragma once

#include <EppoEngine.h>

namespace Eppo
{
    class PanelManager;

    class Panel
    {
    public:
        auto SetPanelManager(PanelManager* panelManager) -> void;

        virtual auto RenderGui() -> void = 0;

    protected:
        auto GetSceneContext() const -> Ref<Scene>;
        auto SetSelectedEntity(Entity entity) -> void;
        auto GetSelectedEntity() const -> Entity;

    private:
        PanelManager* m_PanelManager = nullptr;
    };
}
