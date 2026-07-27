#pragma once

#include "Event/Event.h"

namespace Eppo
{
    class Layer
    {
    public:
        Layer() = default;
        virtual ~Layer() = default;

        virtual auto OnAttach() -> void {}
        virtual auto OnDetach() -> void {}

        virtual auto OnUpdate(float timestep) -> void {}
        virtual auto OnUIRender() -> void {}
        virtual auto OnEvent(Event& e) -> void {}
    };
}