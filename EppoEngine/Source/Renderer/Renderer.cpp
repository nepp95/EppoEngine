#include "pch.h"
#include "Renderer.h"

#include "Renderer/Buffer/IndexBuffer.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RendererContext.h"
#include "Renderer/ShaderLibrary.h"

namespace Eppo
{
	struct RendererData
	{
		Ref<RenderCommandBuffer> CommandBuffer;
		RenderCommandQueue CommandQueue;

		Ref<DescriptorAllocator> DescriptorAllocator;
		Ref<DescriptorLayoutCache> DescriptorCache;
		std::vector<VkDescriptorPool> DescriptorPools;

		// Shaders
		Scope<ShaderLibrary> ShaderLibrary;
	};

	static RendererData* s_Data;

	void Renderer::Init()
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

	void Renderer::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("Renderer::Shutdown");

		s_Data->DescriptorCache->Shutdown();
		s_Data->DescriptorAllocator->Shutdown();

		delete s_Data;
	}

	uint32_t Renderer::GetCurrentFrameIndex()
	{
		Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();

		return swapchain->GetCurrentImageIndex();
	}

	void Renderer::BeginRenderPass(const Ref<RenderCommandBuffer>& renderCommandBuffer, const Ref<Pipeline>& pipeline)
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		uint32_t frameIndex = GetCurrentFrameIndex();
		vkResetDescriptorPool(device, s_Data->DescriptorPools[frameIndex], 0);

		SubmitCommand([renderCommandBuffer, pipeline]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();

			Ref<Framebuffer> framebuffer = pipeline->GetSpecification().Framebuffer;

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = framebuffer->GetExtent();
			renderPassInfo.framebuffer = framebuffer->GetFramebuffer();
			renderPassInfo.renderPass = framebuffer->GetRenderPass();
			renderPassInfo.clearValueCount = framebuffer->GetClearValues().size();
			renderPassInfo.pClearValues = framebuffer->GetClearValues().data();

			vkCmdBeginRenderPass(renderCommandBuffer->GetCurrentCommandBuffer(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		});
	}

	void Renderer::EndRenderPass(const Ref<RenderCommandBuffer>& renderCommandBuffer)
	{
		SubmitCommand([renderCommandBuffer]()
		{
			EPPO_PROFILE_FUNCTION("Renderer::EndRenderPass");

			VkCommandBuffer commandBuffer = renderCommandBuffer->GetCurrentCommandBuffer();
			vkCmdEndRenderPass(commandBuffer);
		});
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

	Ref<Shader> Renderer::GetShader(const std::string& name)
	{
		return s_Data->ShaderLibrary->Get(name);
	}

	Ref<DescriptorAllocator> Renderer::GetDescriptorAllocator()
	{
		return s_Data->DescriptorAllocator;
	}

	Ref<DescriptorLayoutCache> Renderer::GetDescriptorLayoutCache()
	{
		return s_Data->DescriptorCache;
	}

	VkDescriptorSet Renderer::AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo)
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		uint32_t frameIndex = GetCurrentFrameIndex();

		allocInfo.descriptorPool = s_Data->DescriptorPools[frameIndex];

		VkDescriptorSet descriptorSet;
		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet), "Failed to allocate descriptor set!");

		return descriptorSet;
	}

	void Renderer::RenderGeometry(const Ref<RenderCommandBuffer>& renderCommandBuffer, const Ref<Pipeline>& pipeline, const Ref<UniformBuffer>& environmentUB, const Ref<UniformBuffer>& cameraUB, const Ref<Mesh>& mesh, const glm::mat4& transform)
	{
		SubmitCommand([renderCommandBuffer, pipeline, environmentUB, cameraUB, mesh, transform]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();
			VkCommandBuffer commandBuffer = renderCommandBuffer->GetCurrentCommandBuffer();
			uint32_t frameIndex = GetCurrentFrameIndex();
			
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
				VkBuffer vb = { submesh.GetVertexBuffer()->GetBuffer() };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb, offsets);

				// Index buffer
				Ref<IndexBuffer> indexBuffer = submesh.GetIndexBuffer();
				vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

				// Push constants
				vkCmdPushConstants(commandBuffer, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
				vkCmdPushConstants(commandBuffer, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::vec3) + sizeof(float), &mesh->GetMaterial(submesh.GetMaterialIndex()).DiffuseColor);

				// Draw call
				vkCmdDrawIndexed(commandBuffer, indexBuffer->GetIndexCount(), 1, 0, 0, 0);
			}
		});
	}
}
