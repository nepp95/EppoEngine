#include "pch.h"
#include "VulkanRenderer.h"

namespace Eppo
{
	void VulkanRenderer::Init()
	{
		EPPO_PROFILE_FUNCTION("Renderer::Init");

		Ref<RendererContext> context = RendererContext::Get();
		Ref<Swapchain> swapchain = context->GetSwapchain();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		s_Data = new RendererData();
		s_Data->DescriptorAllocator = CreateRef<DescriptorAllocator>();
		s_Data->DescriptorCache = CreateRef<DescriptorLayoutCache>();
		s_Data->CommandBuffer = CreateRef<RenderCommandBuffer>();

		// Load shaders
		s_Data->ShaderLibrary = CreateScope<ShaderLibrary>();
		s_Data->ShaderLibrary->Load("Resources/Shaders/geometry.glsl");
		s_Data->ShaderLibrary->Load("Resources/Shaders/shadow.glsl");

		// Descriptor pool
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 }
		};

		// from IM_ARRAYSIZE (imgui) = ((int)(sizeof(poolSizes) / sizeof(*(poolSizes))));
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.poolSizeCount = ((int)(sizeof(poolSizes) / sizeof(*(poolSizes))));
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = 100 * ((int)(sizeof(poolSizes) / sizeof(*(poolSizes))));

		s_Data->DescriptorPools.resize(VulkanConfig::MaxFramesInFlight);
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &s_Data->DescriptorPools[i]), "Failed to create descriptor pool!");
	}

	void VulkanRenderer::Shutdown()
	{
		throw std::logic_error("The method or operation is not implemented.");
	}
}
