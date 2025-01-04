#pragma once

#include "Renderer/DebugRenderer.h"

namespace Eppo
{
	class VulkanDebugRenderer : public DebugRenderer
	{
	public:
		VulkanDebugRenderer() = default;

		void StartDebugLabel(const Ref<CommandBuffer>& commandBuffer, const std::string& label) override;
		void EndDebugLabel(const Ref<CommandBuffer>& commandBuffer) override;
	};
}
