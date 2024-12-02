#pragma once

#include "Renderer/DebugRenderer.h"

namespace Eppo
{
	class VulkanDebugRenderer : public DebugRenderer
	{
	public:
		VulkanDebugRenderer() = default;

		void StartDebugLabel(Ref<CommandBuffer> commandBuffer, const std::string& label) override;
		void EndDebugLabel(Ref<CommandBuffer> commandBuffer) override;
	};
}
