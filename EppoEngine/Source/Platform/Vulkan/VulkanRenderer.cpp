#include "pch.h"
#include "VulkanRenderer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Platform/Vulkan/VulkanRenderCommandBuffer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"

namespace Eppo
{
	VulkanRenderer::~VulkanRenderer()
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::~VulkanRenderer");

		// TODO: Remove shutdown in favor of destructor?
		Shutdown();
	}

	void VulkanRenderer::Init()
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::Init");

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		Ref<VulkanSwapchain> swapchain = context->GetSwapchain();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		m_DescriptorAllocator = Ref<DescriptorAllocator>::Create();
		m_DescriptorLayoutCache = Ref<DescriptorLayoutCache>::Create();
		m_CommandBuffer = RenderCommandBuffer::Create();
		m_CommandQueue = CreateScope<RenderCommandQueue>();

		// Load shaders
		m_ShaderLibrary = CreateScope<ShaderLibrary>();
		m_ShaderLibrary->Load("Resources/Shaders/geometry.glsl");
		m_ShaderLibrary->Load("Resources/Shaders/shadow.glsl");

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

		m_DescriptorPools.resize(VulkanConfig::MaxFramesInFlight);
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPools[i]), "Failed to create descriptor pool!");
	}

	void VulkanRenderer::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::Shutdown");

		m_DescriptorLayoutCache->Shutdown();
		m_DescriptorAllocator->Shutdown();
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

		vkResetDescriptorPool(device, m_DescriptorPools[frameIndex], 0);

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

		m_CommandQueue->Execute();
	}

	void VulkanRenderer::SubmitCommand(RenderCommand command)
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::SubmitCommand");

		m_CommandQueue->AddCommand(command);
	}

	Ref<Shader> VulkanRenderer::GetShader(const std::string& name)
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::GetShader");

		return m_ShaderLibrary->Get(name);
	}

	void VulkanRenderer::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBuffer> environmentUB, Ref<UniformBuffer> cameraUB, Ref<Mesh> mesh, const glm::mat4& transform)
	{
		SubmitCommand([renderCommandBuffer, pipeline, environmentUB, cameraUB, mesh, transform]()
		{
			EPPO_PROFILE_FUNCTION("VulkanRenderer::RenderGeometry");

			Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
			Ref<VulkanSwapchain> swapchain = context->GetSwapchain();
			VkCommandBuffer commandBuffer = renderCommandBuffer.As<VulkanRenderCommandBuffer>()->GetCurrentCommandBuffer();
			uint32_t frameIndex = swapchain->GetCurrentImageIndex();

			// Pipeline
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());

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

			const auto& descriptorSets = pipeline->GetDescriptorSets(frameIndex);

			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeline->GetPipelineLayout(),
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
				vkCmdPushConstants(commandBuffer, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
				vkCmdPushConstants(commandBuffer, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::vec3) + sizeof(float), &mesh->GetMaterial(submesh.GetMaterialIndex()).DiffuseColor);

				// Draw call
				vkCmdDrawIndexed(commandBuffer, indexBuffer->GetIndexCount(), 1, 0, 0, 0);
			}
		});
	}

	VkDescriptorSet VulkanRenderer::AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo) const
	{
		EPPO_PROFILE_FUNCTION("VulkanRenderer::AllocateDescriptorSet");

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();
		uint32_t frameIndex = GetCurrentFrameIndex();

		allocInfo.descriptorPool = m_DescriptorPools[frameIndex];

		VkDescriptorSet descriptorSet;
		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet), "Failed to allocate descriptor set!");

		return descriptorSet;
	}
}
