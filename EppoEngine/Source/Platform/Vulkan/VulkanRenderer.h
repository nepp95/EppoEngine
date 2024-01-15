#pragma once

#include "Renderer/Renderer.h"

namespace Eppo
{
	class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer() = default;
		~VulkanRenderer() = default;

		void Init() override;
		void Shutdown() override;

	private:
	};
}
