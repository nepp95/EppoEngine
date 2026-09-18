#pragma once

#include <EppoEngine.h>

namespace Eppo
{
    class PanelManager;

    class Panel : public RefCtr
    {
    public:
        virtual ~Panel() = default;

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
