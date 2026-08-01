#pragma once

#include "Panels/Panel.h"

namespace Eppo
{
    class PropertyPanel : public Panel
    {
    public:
        virtual ~PropertyPanel() = default;

        auto RenderGui() -> void override;

    private:
        template<typename T>
        auto DrawAddComponentEntry(const std::string& label) const -> void;

        template<typename T, typename FN>
        auto DrawComponent(Entity entity, FN uiFn, const std::string& tag = {}) -> void;
    };
}
