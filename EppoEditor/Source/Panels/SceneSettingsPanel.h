#pragma once

#include "Panels/Panel.h"

namespace Eppo
{
    class SceneSettingsPanel : public Panel
    {
    public:
        virtual ~SceneSettingsPanel() = default;

        auto RenderGui() -> void override;
    };
}
