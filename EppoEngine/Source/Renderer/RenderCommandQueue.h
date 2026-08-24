#pragma once

namespace Eppo
{
    using RenderCommand = std::function<void()>;

    class RenderCommandQueue
    {
    public:
        RenderCommandQueue() = default;
        ~RenderCommandQueue() = default;
        RenderCommandQueue(const RenderCommandQueue&) = delete;
        auto operator=(const RenderCommandQueue&) -> RenderCommandQueue& = delete;
        RenderCommandQueue(RenderCommandQueue&&) noexcept = default;
        auto operator=(RenderCommandQueue&&) noexcept -> RenderCommandQueue& = default;

        auto AddCommand(RenderCommand&& fn) -> void;
        auto Execute() -> void;
        auto Clear() -> void;

    private:
        std::vector<RenderCommand> m_CommandQueue;
    };
}
