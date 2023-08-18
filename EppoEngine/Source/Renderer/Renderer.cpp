#include "pch.h"
#include "Renderer.h"

#include "Renderer/Buffer/IndexBuffer.h"
#include "Renderer/Buffer/UniformBuffer.h"
#include "Renderer/Buffer/VertexBuffer.h"
#include "Renderer/Descriptor/DescriptorBuilder.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RendererContext.h"
#include "Renderer/Vertex.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Eppo
{
	struct RendererData
	{
		static const uint32_t MaxQuads = 20000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32;

		RenderCommandQueue CommandQueue;

		Ref<DescriptorAllocator> DescriptorAllocator;
		Ref<DescriptorLayoutCache> DescriptorCache;

		// Quad
		Vertex* QuadVertexBufferBase = nullptr;
		Vertex* QuadVertexBufferPtr = nullptr;
		Ref<Pipeline> QuadPipeline;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<IndexBuffer> QuadIndexBuffer;
		Ref<Material> QuadMaterial;
		uint32_t QuadIndexCount = 0;

		glm::vec4 QuadVertexPositions[4];
		glm::vec2 QuadVertexTexCoords[4];

		// Textures
		Ref<Texture> WhiteTexture;
		std::array<Ref<Texture>, MaxTextureSlots> TextureSlots;
		uint32_t TextureSlotIndex = 1;

		// Camera
		struct CameraData 
		{
			glm::mat4 ViewProjection;
		};
		CameraData CameraBuffer;
		Ref<UniformBuffer> CameraUniformBuffer;
	};

	static RendererData* s_Data;

	void Renderer::Init()
	{
		EPPO_PROFILE_FUNCTION("Renderer::Init");

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		s_Data = new RendererData();
		s_Data->DescriptorAllocator = CreateRef<DescriptorAllocator>();
		s_Data->DescriptorCache = CreateRef<DescriptorLayoutCache>();

		ShaderSpecification quadShaderSpec;
		quadShaderSpec.ShaderSources = {
			{ ShaderType::Vertex, "Resources/Shaders/quad.vert" },
			{ ShaderType::Fragment, "Resources/Shaders/quad.frag" },
		};

		PipelineSpecification quadPipelineSpec;
		quadPipelineSpec.Shader = CreateRef<Shader>(quadShaderSpec, s_Data->DescriptorCache);

		s_Data->QuadPipeline = CreateRef<Pipeline>(quadPipelineSpec);

		// Vertex buffer
		s_Data->QuadVertexBuffer = CreateRef<VertexBuffer>(sizeof(Vertex) * RendererData::MaxVertices);
		s_Data->QuadVertexBufferBase = new Vertex[RendererData::MaxVertices];

		s_Data->QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		s_Data->QuadVertexTexCoords[0] = { 0.0f, 0.0f };
		s_Data->QuadVertexTexCoords[1] = { 1.0f, 0.0f };
		s_Data->QuadVertexTexCoords[2] = { 1.0f, 1.0f };
		s_Data->QuadVertexTexCoords[3] = { 0.0f, 1.0f };

		// Index buffer
		uint32_t* quadIndices = new uint32_t[RendererData::MaxIndices];
		uint32_t offset = 0;
		for (uint32_t i = 0; i < RendererData::MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 2;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 0;

			quadIndices[i + 3] = offset + 0;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 2;

			offset += 4;
		}

		s_Data->QuadIndexBuffer = CreateRef<IndexBuffer>((void*)quadIndices, RendererData::MaxIndices);
		delete[] quadIndices;

		// Textures
		uint32_t whiteTextureData = 0xffffffff;
		s_Data->WhiteTexture = CreateRef<Texture>(1, 1, ImageFormat::RGBA8, &whiteTextureData);
		s_Data->TextureSlots[0] = s_Data->WhiteTexture;

		// Materials
		s_Data->QuadMaterial = CreateRef<Material>(quadPipelineSpec.Shader);

		// Camera
		s_Data->CameraUniformBuffer = CreateRef<UniformBuffer>(quadPipelineSpec.Shader, sizeof(RendererData::CameraBuffer));
	}

	void Renderer::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("Renderer::Shutdown");

		s_Data->DescriptorCache->Shutdown();
		s_Data->DescriptorAllocator->Shutdown();

		delete s_Data;
	}

	void Renderer::BeginFrame()
	{
		SubmitCommand([]()
		{
			EPPO_PROFILE_FUNCTION("Renderer::BeginFrame");

			Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();
			VkCommandBuffer commandBuffer = swapchain->GetCurrentRenderCommandBuffer();

			uint32_t imageIndex = swapchain->GetCurrentImageIndex();

			//s_Data->DescriptorAllocator->ResetPools();

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0;
			beginInfo.pInheritanceInfo = nullptr;

			VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer!");
		});
	}

	void Renderer::EndFrame()
	{
		SubmitCommand([]()
		{
			EPPO_PROFILE_FUNCTION("Renderer::EndFrame");

			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();
			VkCommandBuffer commandBuffer = swapchain->GetCurrentRenderCommandBuffer();

			EPPO_PROFILE_GPU_END(context->GetCurrentProfilerContext(), commandBuffer);

			VK_CHECK(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer!");
		});
	}


	void Renderer::BeginRenderPass()
	{
		SubmitCommand([]()
		{
			EPPO_PROFILE_FUNCTION("Renderer::BeginRenderPass");

			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();

			EPPO_PROFILE_GPU(context->GetCurrentProfilerContext(), swapchain->GetCurrentRenderCommandBuffer(), "Render pass");

			// Render
			VkExtent2D extent = swapchain->GetExtent();

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = swapchain->GetRenderPass();
			renderPassInfo.framebuffer = swapchain->GetCurrentFramebuffer();
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = extent;

			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			vkCmdBeginRenderPass(swapchain->GetCurrentRenderCommandBuffer(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		});
	}


	void Renderer::EndRenderPass()
	{
		SubmitCommand([]()
		{
			EPPO_PROFILE_FUNCTION("Renderer::EndRenderPass");

			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();

			vkCmdEndRenderPass(swapchain->GetCurrentRenderCommandBuffer());
		});
	}

	void Renderer::StartBatch()
	{
		EPPO_PROFILE_FUNCTION("Renderer::StartBatch");

		s_Data->QuadIndexCount = 0;
		s_Data->QuadVertexBufferPtr = s_Data->QuadVertexBufferBase;

		s_Data->TextureSlotIndex = 1;
	}


	void Renderer::NextBatch()
	{
		EPPO_PROFILE_FUNCTION("Renderer::NextBatch");

		Flush();
		StartBatch();
	}


	void Renderer::Flush()
	{
		EPPO_PROFILE_FUNCTION("Renderer::Flush");

		BeginRenderPass();

		// Quads
		uint32_t dataSize = (uint32_t)((uint8_t*)s_Data->QuadVertexBufferPtr - (uint8_t*)s_Data->QuadVertexBufferBase);
		if (dataSize)
		{
			SubmitCommand([dataSize]()
			{
				s_Data->QuadVertexBuffer->SetData(s_Data->QuadVertexBufferBase, dataSize);

				Ref<RendererContext> context = RendererContext::Get();
				Ref<Swapchain> swapchain = context->GetSwapchain();

				VkCommandBuffer commandBuffer = swapchain->GetCurrentRenderCommandBuffer();

				// Update descriptors
				for (uint32_t i = 0; i < s_Data->TextureSlots.size(); i++)
				{
					if (s_Data->TextureSlots[i])
						s_Data->QuadMaterial->Set("texSampler", s_Data->TextureSlots[i], i);
					else
						s_Data->QuadMaterial->Set("texSampler", s_Data->WhiteTexture, i);
				}
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_Data->QuadPipeline->GetPipeline());

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

				VkBuffer vbo[] = { s_Data->QuadVertexBuffer->GetBuffer() };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vbo, offsets);
				vkCmdBindIndexBuffer(commandBuffer, s_Data->QuadIndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

				{
					VkDescriptorSet descriptorSet = s_Data->CameraUniformBuffer->GetCurrentDescriptorSet();
					vkCmdBindDescriptorSets(
						commandBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						s_Data->QuadPipeline->GetPipelineLayout(),
						1,
						1,
						&descriptorSet,
						0,
						nullptr
					);
				}

				{
					VkDescriptorSet descriptorSet = s_Data->QuadMaterial->GetCurrentDescriptorSet();
					vkCmdBindDescriptorSets(
						commandBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						s_Data->QuadPipeline->GetPipelineLayout(),
						2,
						1,
						&descriptorSet,
						0,
						nullptr
					);
				}

				// Draw
				vkCmdDrawIndexed(commandBuffer, s_Data->QuadIndexCount, 1, 0, 0, 0);
			});
		}

		EndRenderPass();
	}

	void Renderer::BeginScene(const EditorCamera& camera)
	{
		EPPO_PROFILE_FUNCTION("Renderer::BeginScene");

		Ref<RendererContext> context = RendererContext::Get();
		Ref<Swapchain> swapchain = context->GetSwapchain();

		uint32_t imageIndex = swapchain->GetCurrentImageIndex();

		s_Data->CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
		s_Data->CameraUniformBuffer->SetData(&s_Data->CameraBuffer, sizeof(RendererData::CameraBuffer));

		StartBatch();
	}

	void Renderer::EndScene()
	{
		EPPO_PROFILE_FUNCTION("Renderer::EndScene");

		Flush();
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

	Ref<DescriptorAllocator> Renderer::GetDescriptorAllocator()
	{
		return s_Data->DescriptorAllocator;
	}

	Ref<DescriptorLayoutCache> Renderer::GetDescriptorLayoutCache()
	{
		return s_Data->DescriptorCache;
	}

	void Renderer::DrawQuad(const glm::vec2& position, const glm::vec4& color)
	{
		EPPO_PROFILE_FUNCTION("Renderer::DrawQuad");

		DrawQuad({ position.x, position.y, 0.0f }, color);
	}
	
	void Renderer::DrawQuad(const glm::vec3& position, const glm::vec4& color)
	{
		EPPO_PROFILE_FUNCTION("Renderer::DrawQuad");

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
		DrawQuad(transform, color);
	}

	void Renderer::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		EPPO_PROFILE_FUNCTION("Renderer::DrawQuad");

		if (s_Data->QuadIndexCount + 6 >= RendererData::MaxIndices)
			return; // TODO: Next batch

		for (uint32_t i = 0; i < 4; i++)
		{
			s_Data->QuadVertexBufferPtr->Position = transform * s_Data->QuadVertexPositions[i];
			s_Data->QuadVertexBufferPtr->Color = color;
			s_Data->QuadVertexBufferPtr->TexCoord = s_Data->QuadVertexTexCoords[i];
			s_Data->QuadVertexBufferPtr->TexIndex = 0.0f;
			s_Data->QuadVertexBufferPtr++;
		}
		const auto& data = s_Data;
		s_Data->QuadIndexCount += 6;
	}

	void Renderer::DrawQuad(const glm::vec2& position, Ref<Texture> texture, const glm::vec4& tintColor)
	{
		EPPO_PROFILE_FUNCTION("Renderer::DrawQuad");

		DrawQuad({ position.x, position.y, 0.0f }, texture, tintColor);
	}

	void Renderer::DrawQuad(const glm::vec3& position, Ref<Texture> texture, const glm::vec4& tintColor)
	{
		EPPO_PROFILE_FUNCTION("Renderer::DrawQuad");

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
		DrawQuad(transform, texture, tintColor);
	}

	void Renderer::DrawQuad(const glm::mat4& transform, Ref<Texture> texture, const glm::vec4& tintColor)
	{
		EPPO_PROFILE_FUNCTION("Renderer::DrawQuad");

		if (s_Data->QuadIndexCount + 6 >= RendererData::MaxIndices)
			return; // TODO: Next batch

		float textureIndex = 0.0f;
		for (uint32_t i = 0; i < s_Data->TextureSlotIndex; i++)
		{
			if (s_Data->TextureSlots[i] == texture)
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			if (s_Data->TextureSlotIndex >= RendererData::MaxTextureSlots)
				return; // TODO: Next batch

			textureIndex = (float)s_Data->TextureSlotIndex;
			s_Data->TextureSlots[s_Data->TextureSlotIndex] = texture;
			s_Data->TextureSlotIndex++;
		}

		for (uint32_t i = 0; i < 4; i++)
		{
			s_Data->QuadVertexBufferPtr->Position = transform * s_Data->QuadVertexPositions[i];
			s_Data->QuadVertexBufferPtr->Color = tintColor;
			s_Data->QuadVertexBufferPtr->TexCoord = s_Data->QuadVertexTexCoords[i];
			s_Data->QuadVertexBufferPtr->TexIndex = textureIndex;
			s_Data->QuadVertexBufferPtr++;
		}

		s_Data->QuadIndexCount += 6;
	}
}
