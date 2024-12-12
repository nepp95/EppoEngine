#pragma once

#include <queue>

namespace Eppo
{
	using RenderCommand = std::function<void()>;

	class CommandQueue
	{
	public:
		explicit CommandQueue(bool isMultiThreaded = false);
		~CommandQueue() = default;

		void AddCommand(RenderCommand fn);
		void Execute();

	private:
		std::queue<RenderCommand> m_CommandQueue;
		uint32_t m_CommandCount = 0;

		bool m_IsMultiThreaded;
		std::mutex m_Mutex;
	};
}
