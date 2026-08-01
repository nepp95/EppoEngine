#pragma once

#include "Panels/Panel.h"

namespace Eppo
{
    class SceneHierarchyPanel : public Panel
    {
    public:
        virtual ~SceneHierarchyPanel() = default;

        auto RenderGui() -> void override;

    private:
        auto DrawEntityNode(Entity entity) -> void;
    };
}
