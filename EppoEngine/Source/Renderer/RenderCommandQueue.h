#pragma once

#include <queue>

namespace Eppo
{
	using RenderCommand = std::function<void()>;

	class RenderCommandQueue
	{
	public:
		RenderCommandQueue() = default;
		~RenderCommandQueue() = default;

		void AddCommand(RenderCommand fn);
		void Execute();

	private:
		std::queue<RenderCommand> m_CommandQueue;
		uint32_t m_CommandCount = 0;
	};
}
