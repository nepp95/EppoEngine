#include "pch.h"
#include "Renderer.h"

#include "Renderer/DescriptorAllocator.h"
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
		EPPO_PROFILE_FUNCTION("Renderer::Init");

		Ref<RendererContext> context = RendererContext::Get();
		Ref<LogicalDevice> logicalDevice = context->GetLogicalDevice();
		Ref<Swapchain> swapchain = context->GetSwapchain();

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
		EPPO_PROFILE_FUNCTION("Renderer::Shutdown");

		for (auto& allocator : s_Data->DescriptorAllocators)
		{
			EPPO_WARN("Releasing descriptor pool {}", (void*)&allocator);
			allocator.DestroyPools();
		}

		delete s_Data;
	}

	uint32_t Renderer::GetCurrentFrameIndex()
	{
		return RendererContext::Get()->GetSwapchain()->GetCurrentImageIndex();
	}

	void Renderer::ExecuteRenderCommands()
	{
		EPPO_PROFILE_FUNCTION("Renderer::ExecuteRenderCommands");

		s_Data->CommandQueue.Execute();
	}

	void Renderer::SubmitCommand(RenderCommand command)
	{
		EPPO_PROFILE_FUNCTION("Renderer::SubmitCommand");

		s_Data->CommandQueue.AddCommand(command);
	}

	void Renderer::RT_BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline)
	{
		SubmitCommand([renderCommandBuffer, pipeline]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();

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

				VkCommandBuffer commandBuffer = renderCommandBuffer->GetCurrentCommandBuffer();
				vkCmdBeginRendering(commandBuffer, &renderingInfo);
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
					attachmentInfo.imageView = pipeline->GetImage(i)->GetImageInfo().ImageView;
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
					attachmentInfo.imageView = spec.DepthImage->GetImageInfo().ImageView;

					renderingInfo.pDepthAttachment = &attachmentInfo;

					if (spec.DepthImage->GetSpecification().CubeMap)
						renderingInfo.viewMask = 0b111111;
				}

				VkCommandBuffer commandBuffer = renderCommandBuffer->GetCurrentCommandBuffer();
				vkCmdBeginRendering(commandBuffer, &renderingInfo);
			}
		});
	}

	void Renderer::RT_EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		SubmitCommand([renderCommandBuffer]()
		{
			VkCommandBuffer commandBuffer = renderCommandBuffer->GetCurrentCommandBuffer();
			vkCmdEndRendering(commandBuffer);
		});
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
