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
        for (size_t i = 0; i < m_CommandQueue.size(); i++)
            m_CommandQueue.at(i)();

        m_CommandQueue.clear();
    }

    auto RenderCommandQueue::Clear() -> void
    {
        m_CommandQueue.clear();
    }
}
