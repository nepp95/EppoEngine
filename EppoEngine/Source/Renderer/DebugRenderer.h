#pragma once

#include "Renderer/CommandBuffer.h"

namespace Eppo
{
	class DebugRenderer
	{
	public:
		virtual ~DebugRenderer() = default;

		virtual void StartDebugLabel(const Ref<CommandBuffer>& commandBuffer, const std::string& label) = 0;
		virtual void EndDebugLabel(const Ref<CommandBuffer>& commandBuffer) = 0;

		static Ref<DebugRenderer> Create();
	};
}
