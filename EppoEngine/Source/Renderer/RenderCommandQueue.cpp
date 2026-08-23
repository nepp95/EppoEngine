#include "pch.h"
#include "Renderer/RenderCommandQueue.h"

namespace Eppo
{
    auto RenderCommandQueue::AddCommand(RenderCommand&& fn) -> void
    {
        m_CommandQueue.emplace_back(std::move(fn));
    }

    auto RenderCommandQueue::Execute() -> void
    {
        std::vector<RenderCommand> commands;
        commands.swap(m_CommandQueue);

        for (auto& command : commands)
            command();
    }

    auto RenderCommandQueue::Clear() -> void
    {
        m_CommandQueue.clear();
    }
}
