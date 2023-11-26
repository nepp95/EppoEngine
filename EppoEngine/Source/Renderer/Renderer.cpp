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
		s_Data->ShaderLibrary = CreateScope<ShaderLibrary>();

		s_Data->ShaderLibrary->Load("Resources/Shaders/geometry.glsl");
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

	void Renderer::RenderGeometry(const Ref<RenderCommandBuffer>& renderCommandBuffer, const Ref<Pipeline>& pipeline, const Ref<UniformBuffer>& cameraBuffer, const Ref<Mesh>& mesh, const glm::mat4& transform)
	{
		SubmitCommand([renderCommandBuffer, pipeline, cameraBuffer, mesh, transform]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();

			VkCommandBuffer commandBuffer = renderCommandBuffer->GetCurrentCommandBuffer();
			
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

			VkDescriptorSet descriptorSet = cameraBuffer->GetCurrentDescriptorSet();
			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeline->GetPipelineLayout(),
				1,
				1,
				&descriptorSet,
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
