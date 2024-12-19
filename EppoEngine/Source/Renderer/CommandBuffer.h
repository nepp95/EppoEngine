#pragma once

namespace Eppo
{
	class CommandBuffer
	{
	public:
		virtual ~CommandBuffer() = default;

		virtual void RT_Begin() = 0;
		virtual void RT_End() = 0;
		virtual void RT_Submit() const = 0;

		static Ref<CommandBuffer> Create(bool manualSubmission = true, uint32_t count = 0);
	};
}
