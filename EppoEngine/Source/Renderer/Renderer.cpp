#include "pch.h"
#include "Renderer.h"

#include "Platform/Vulkan/DescriptorAllocator.h"
#include "Platform/Vulkan/VulkanImage.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/RendererContext.h"
#include "Renderer/ShaderLibrary.h"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/gtc/type_ptr.hpp>

namespace Eppo
{
	struct RendererData
	{
		RenderCommandQueue CommandQueue;
		std::vector<DescriptorAllocator> DescriptorAllocators;
		Scope<ShaderLibrary> ShaderLibrary;

		// Stats
		struct RenderStatistics
		{
			uint32_t DrawCalls = 0;
			uint32_t Meshes = 0;
			uint32_t Submeshes = 0;
		} RenderStatistics;
	};

	static RendererData* s_Data;

	void Renderer::Init()
	{
		Ref<VulkanContext> context = VulkanContext::Get();
		Ref<VulkanLogicalDevice> logicalDevice = context->GetLogicalDevice();
		Ref<VulkanSwapchain> swapchain = context->GetSwapchain();

		s_Data = new RendererData();

		// Create descriptor allocators
		std::vector<DescriptorAllocator::PoolSizeRatio> ratios = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3.0f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3.0f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3.0f },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.0f}
		};

		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			DescriptorAllocator& allocator = s_Data->DescriptorAllocators.emplace_back();
			allocator.Init(1000, ratios);
		}

		// Load shaders
		s_Data->ShaderLibrary = CreateScope<ShaderLibrary>();
		s_Data->ShaderLibrary->Load("Resources/Shaders/composite.glsl");
		s_Data->ShaderLibrary->Load("Resources/Shaders/debug.glsl");
		s_Data->ShaderLibrary->Load("Resources/Shaders/geometry.glsl");
		s_Data->ShaderLibrary->Load("Resources/Shaders/predepth.glsl");
	}

	void Renderer::Shutdown()
	{
		for (auto& allocator : s_Data->DescriptorAllocators)
		{
			EPPO_WARN("Releasing descriptor pool {}", (void*)&allocator);
			allocator.DestroyPools();
		}

		delete s_Data;
	}

	uint32_t Renderer::GetCurrentFrameIndex()
	{
		return VulkanContext::Get()->GetSwapchain()->GetCurrentImageIndex();
	}

	void Renderer::ExecuteRenderCommands()
	{
		EPPO_PROFILE_FUNCTION("Renderer::ExecuteRenderCommands");

		s_Data->CommandQueue.Execute();
	}

	void Renderer::SubmitCommand(RenderCommand command)
	{
		s_Data->CommandQueue.AddCommand(command);
	}

	void Renderer::BeginRenderPass(Ref<CommandBuffer> commandBuffer, Ref<Pipeline> pipeline)
	{
		EPPO_PROFILE_FUNCTION("Renderer::BeginRenderPass");

		Ref<VulkanContext> context = VulkanContext::Get();
		Ref<VulkanSwapchain> swapchain = context->GetSwapchain();

		const auto& spec = pipeline->GetSpecification();

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea.offset = { 0, 0 };
		renderingInfo.renderArea.extent = { spec.Width, spec.Height };
		renderingInfo.layerCount = 1;

		if (spec.SwapchainTarget)
		{
			VkRenderingAttachmentInfo attachmentInfo{};
			attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachmentInfo.imageView = swapchain->GetCurrentImageView();

			const glm::vec4& cv = spec.ColorAttachments[0].ClearValue;
			attachmentInfo.clearValue = { cv.r, cv.g, cv.b, cv.a };

			renderingInfo.colorAttachmentCount = 1;
			renderingInfo.pColorAttachments = &attachmentInfo;

			auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
			VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();
			vkCmdBeginRendering(cb, &renderingInfo);
		} else
		{
			std::vector<VkRenderingAttachmentInfo> attachmentInfos;

			for (uint32_t i = 0; i < spec.ColorAttachments.size(); i++)
			{
				const auto& attachment = spec.ColorAttachments[i];

				VkRenderingAttachmentInfo& attachmentInfo = attachmentInfos.emplace_back();
				attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachmentInfo.loadOp = attachment.Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
				attachmentInfo.clearValue = { attachment.ClearValue.r, attachment.ClearValue.g, attachment.ClearValue.b, attachment.ClearValue.a };
				attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				attachmentInfo.imageView = std::static_pointer_cast<VulkanImage>(pipeline->GetImage(i))->GetImageInfo().ImageView;
			}

			renderingInfo.colorAttachmentCount = static_cast<uint32_t>(attachmentInfos.size());
			renderingInfo.pColorAttachments = attachmentInfos.data();

			if (spec.DepthImage)
			{
				VkRenderingAttachmentInfo attachmentInfo{};
				attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachmentInfo.loadOp = spec.ClearDepthOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
				attachmentInfo.clearValue = { spec.ClearDepth, 0 };
				attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
				attachmentInfo.imageView = std::static_pointer_cast<VulkanImage>(spec.DepthImage)->GetImageInfo().ImageView;

				renderingInfo.pDepthAttachment = &attachmentInfo;

				if (spec.DepthImage->GetSpecification().CubeMap)
					renderingInfo.viewMask = 0b111111;
			}

			auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
			VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();
			vkCmdBeginRendering(cb, &renderingInfo);
		}
	}

	void Renderer::EndRenderPass(Ref<CommandBuffer> commandBuffer)
	{
		EPPO_PROFILE_FUNCTION("Renderer::EndRenderPass");

		auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
		VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();
		vkCmdEndRendering(cb);
	}

	Ref<Shader> Renderer::GetShader(const std::string& name)
	{
		return s_Data->ShaderLibrary->Get(name);
	}

	VkDescriptorSet Renderer::AllocateDescriptor(VkDescriptorSetLayout layout)
	{
		uint32_t frameIndex = GetCurrentFrameIndex();
		return s_Data->DescriptorAllocators[frameIndex].Allocate(layout);
	}
}
