#include "pch.h"
#include "VulkanRenderer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanRenderCommandBuffer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShaderLibrary.h"

namespace Eppo
{
	struct RendererData
	{
		Scope<RenderCommandQueue> CommandQueue;
		Ref<RenderCommandBuffer> CommandBuffer;

		Ref<DescriptorAllocator> DescriptorAllocator;
		Ref<DescriptorLayoutCache> DescriptorLayoutCache;
		std::vector<VkDescriptorPool> DescriptorPools;

		Scope<ShaderLibrary> ShaderLibrary;
	};

	static RendererData* s_Data = nullptr;

	VulkanRenderer::~VulkanRenderer()
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::~VulkanRenderer");

		// TODO: Remove shutdown in favor of destructor?
		Shutdown();

		delete s_Data;
	}

	void VulkanRenderer::Init()
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::Init");

		s_Data = new RendererData();

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		Ref<VulkanSwapchain> swapchain = context->GetSwapchain();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		s_Data->DescriptorAllocator = Ref<DescriptorAllocator>::Create();
		s_Data->DescriptorLayoutCache = Ref<DescriptorLayoutCache>::Create();
		s_Data->CommandBuffer = RenderCommandBuffer::Create();
		s_Data->CommandQueue = CreateScope<RenderCommandQueue>();

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
		EPPO_PROFILE_FUNCTION("VulkanRenderer::Shutdown");

		s_Data->DescriptorLayoutCache->Shutdown();
		s_Data->DescriptorAllocator->Shutdown();
	}

	uint32_t VulkanRenderer::GetCurrentFrameIndex() const
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::GetCurrentFrameIndex");

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		Ref<VulkanSwapchain> swapchain = context->GetSwapchain();

		return swapchain->GetCurrentImageIndex();
	}

	void VulkanRenderer::BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline)
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::BeginRenderPass");

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();
		uint32_t frameIndex = GetCurrentFrameIndex();

		vkResetDescriptorPool(device, s_Data->DescriptorPools[frameIndex], 0);

		SubmitCommand([renderCommandBuffer, pipeline]()
		{
			Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
			Ref<VulkanSwapchain> swapchain = context->GetSwapchain();

			Ref<VulkanFramebuffer> framebuffer = pipeline->GetSpecification().Framebuffer.As<VulkanFramebuffer>();

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = framebuffer->GetExtent();
			renderPassInfo.framebuffer = framebuffer->GetFramebuffer();
			renderPassInfo.renderPass = framebuffer->GetRenderPass();
			renderPassInfo.clearValueCount = framebuffer->GetClearValues().size();
			renderPassInfo.pClearValues = framebuffer->GetClearValues().data();

			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCurrentCommandBuffer();
			vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		});
	}

	void VulkanRenderer::EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		SubmitCommand([renderCommandBuffer]()
		{
			EPPO_PROFILE_FUNCTION("VulkanRenderer::EndRenderPass");

			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCurrentCommandBuffer();
			vkCmdEndRenderPass(commandBuffer);
		});
	}

	void VulkanRenderer::ExecuteRenderCommands()
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::ExecuteRenderCommands");

		s_Data->CommandQueue->Execute();
	}

	void VulkanRenderer::SubmitCommand(RenderCommand command)
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::SubmitCommand");

		s_Data->CommandQueue->AddCommand(command);
	}

	Ref<Shader> VulkanRenderer::GetShader(const std::string& name)
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::GetShader");

		return s_Data->ShaderLibrary->Get(name);
	}

	void VulkanRenderer::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<Mesh> mesh, const glm::mat4& transform)
	{
		SubmitCommand([renderCommandBuffer, pipeline, uniformBufferSet, mesh, transform]()
		{
			EPPO_PROFILE_FUNCTION("VulkanRenderer::RenderGeometry");

			Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
			Ref<VulkanSwapchain> swapchain = context->GetSwapchain();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCurrentCommandBuffer();
			uint32_t frameIndex = swapchain->GetCurrentImageIndex();

			// Pipeline
			Ref<VulkanPipeline> vulkanPipeline = pipeline.As<VulkanPipeline>();
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline->GetPipeline());

			VkExtent2D extent = swapchain->GetExtent();

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)extent.width;
			viewport.height = (float)extent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = extent;
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			const auto& descriptorSets = vulkanPipeline->GetDescriptorSets(frameIndex);

			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				vulkanPipeline->GetPipelineLayout(),
				0,
				1,
				&descriptorSets[0],
				0,
				nullptr
			);

			for (const auto& submesh : mesh->GetSubmeshes())
			{
				// Vertex buffer Mesh
				VkBuffer vb = { submesh.GetVertexBuffer().As<VulkanVertexBuffer>()->GetBuffer() };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb, offsets);

				// Index buffer
				Ref<VulkanIndexBuffer> indexBuffer = submesh.GetIndexBuffer().As<VulkanIndexBuffer>();
				vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

				// Push constants
				vkCmdPushConstants(commandBuffer, vulkanPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
				vkCmdPushConstants(commandBuffer, vulkanPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::vec3) + sizeof(float), &mesh->GetMaterial(submesh.GetMaterialIndex()).DiffuseColor);

				// Draw call
				vkCmdDrawIndexed(commandBuffer, indexBuffer->GetIndexCount(), 1, 0, 0, 0);
			}
		});
	}

	Ref<DescriptorAllocator> VulkanRenderer::GetDescriptorAllocator()
	{
		return s_Data->DescriptorAllocator;
	}

	Ref<DescriptorLayoutCache> VulkanRenderer::GetDescriptorLayoutCache()
	{
		return s_Data->DescriptorLayoutCache;
	}

	VkDescriptorSet VulkanRenderer::AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo)
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::AllocateDescriptorSet");

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		allocInfo.descriptorPool = s_Data->DescriptorPools[frameIndex];

		VkDescriptorSet descriptorSet;
		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet), "Failed to allocate descriptor set!");

		return descriptorSet;
	}
}
