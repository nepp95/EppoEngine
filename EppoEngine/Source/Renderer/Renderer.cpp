#include "pch.h"
#include "Renderer.h"

#include "Renderer/Buffers/IndexBuffer.h"
#include "Renderer/Buffers/VertexBuffer.h"
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

		std::array<VkDescriptorPool, VulkanConfig::MaxFramesInFlight> DescriptorPools;

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
	};

	static RendererData* s_Data;

	void Renderer::Init()
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		s_Data = new RendererData();

		ShaderSpecification quadShaderSpec;
		quadShaderSpec.ShaderSources = {
			{ ShaderType::Vertex, "Resources/Shaders/quad.vert" },
			{ ShaderType::Fragment, "Resources/Shaders/quad.frag" },
		};

		PipelineSpecification quadPipelineSpec;
		quadPipelineSpec.Shader = CreateRef<Shader>(quadShaderSpec);

		s_Data->QuadPipeline = CreateRef<Pipeline>(quadPipelineSpec);

		// Vertex buffer
		s_Data->QuadVertexBuffer = CreateRef<VertexBuffer>(sizeof(Vertex) * RendererData::MaxVertices);

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
		uint32_t whiteTextureData = 0xfff000fff;
		s_Data->WhiteTexture = CreateRef<Texture>(1, 1, ImageFormat::RGBA8, &whiteTextureData);
		s_Data->TextureSlots[0] = s_Data->WhiteTexture;

		// Descriptor pools
		// TODO: Descriptor Abstraction
		VkDescriptorPoolSize poolSizes[] = {
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
		
		uint32_t poolSizeCount = ((int)(sizeof(poolSizes) / sizeof(*(poolSizes)))); // IM_ARRAYSIZE from imgui

		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			VkDescriptorPoolCreateInfo poolCreateInfo{};
			poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolCreateInfo.maxSets = 1000 * poolSizeCount;
			poolCreateInfo.poolSizeCount = poolSizeCount;
			poolCreateInfo.pPoolSizes = poolSizes;
			poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

			VK_CHECK(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &s_Data->DescriptorPools[i]), "Failed to create descriptor pool!");
		}

		s_Data->QuadMaterial = CreateRef<Material>(s_Data->QuadPipeline->GetSpecification().Shader);
	}

	void Renderer::Shutdown()
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		for (auto& pool : s_Data->DescriptorPools)
			vkDestroyDescriptorPool(device, pool, nullptr);

		delete s_Data;
	}

	void Renderer::BeginFrame()
	{
		SubmitCommand([]()
		{
			Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();
			VkCommandBuffer commandBuffer = swapchain->GetCurrentRenderCommandBuffer();

			uint32_t imageIndex = swapchain->GetCurrentImageIndex();

			VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
			vkResetDescriptorPool(device, s_Data->DescriptorPools[imageIndex], 0);

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
			Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();
			VkCommandBuffer commandBuffer = swapchain->GetCurrentRenderCommandBuffer();

			VK_CHECK(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer!");
		});
	}

	void Renderer::BeginScene()
	{
		Renderer::SubmitCommand([]()
		{
			s_Data->QuadIndexCount = 0;
			s_Data->QuadVertexBuffer->Reset();
		});
	}

	void Renderer::EndScene()
	{
		SubmitCommand([]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();

			// Update descriptors
			for (uint32_t i = 0; i < s_Data->TextureSlots.size(); i++)
			{
				if (s_Data->TextureSlots[i])
					s_Data->QuadMaterial->Set("texSampler", s_Data->TextureSlots[i], i);
				else
					s_Data->QuadMaterial->Set("texSampler", s_Data->WhiteTexture, i);
			}

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
			vkCmdBindPipeline(swapchain->GetCurrentRenderCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_Data->QuadPipeline->GetPipeline());

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)extent.width;
			viewport.height = (float)extent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(swapchain->GetCurrentRenderCommandBuffer(), 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = extent;
			vkCmdSetScissor(swapchain->GetCurrentRenderCommandBuffer(), 0, 1, &scissor);

			VkBuffer vbo[] = { s_Data->QuadVertexBuffer->GetBuffer() };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(swapchain->GetCurrentRenderCommandBuffer(), 0, 1, vbo, offsets);
			vkCmdBindIndexBuffer(swapchain->GetCurrentRenderCommandBuffer(), s_Data->QuadIndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

			VkDescriptorSet descriptorSet = s_Data->QuadMaterial->Get();
			vkCmdBindDescriptorSets(
				swapchain->GetCurrentRenderCommandBuffer(),
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				s_Data->QuadPipeline->GetPipelineLayout(),
				2,
				1,
				&descriptorSet,
				0,
				nullptr
			);

			// Draw
			vkCmdDrawIndexed(swapchain->GetCurrentRenderCommandBuffer(), s_Data->QuadIndexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(swapchain->GetCurrentRenderCommandBuffer());
		});
	}

	void Renderer::ExecuteRenderCommands()
	{
		s_Data->CommandQueue.Execute();
	}

	void Renderer::SubmitCommand(RenderCommand command)
	{
		s_Data->CommandQueue.AddCommand(command);
	}

	VkDescriptorSet Renderer::AllocateDescriptorSet(const VkDescriptorSetLayout& layout)
	{
		Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = s_Data->DescriptorPools[swapchain->GetCurrentImageIndex()];
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout;
		allocInfo.pNext = nullptr;

		VkDescriptorSet descriptorSet;

		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet), "Failed to allocate descriptor set!");

		return descriptorSet;
	}

	void Renderer::UpdateDescriptorSet(Ref<Texture> texture, VkDescriptorSet descriptorSet, uint32_t arrayElement)
	{
		const auto& info = texture->GetImage()->GetImageInfo();
		
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = info.ImageLayout;
		imageInfo.imageView = info.ImageView;
		imageInfo.sampler = info.Sampler;

		VkWriteDescriptorSet writeDesc{};
		writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDesc.dstSet = descriptorSet;
		writeDesc.dstBinding = 1;
		writeDesc.dstArrayElement = arrayElement;
		writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDesc.descriptorCount = 1;
		writeDesc.pBufferInfo = nullptr;
		writeDesc.pImageInfo = &imageInfo;
		writeDesc.pTexelBufferView = nullptr;

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		vkUpdateDescriptorSets(device, 1, &writeDesc, 0, nullptr);
	}

	void Renderer::DrawQuad(const glm::vec2& position, const glm::vec4& color)
	{
		DrawQuad({ position.x, position.y, 0.0f }, color);
	}
	
	void Renderer::DrawQuad(const glm::vec3& position, const glm::vec4& color)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
		DrawQuad(transform, color);
	}

	void Renderer::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		Renderer::SubmitCommand([=]()
		{
			if (s_Data->QuadIndexCount + 6 >= RendererData::MaxIndices)
				return; // TODO: Next batch

			Vertex vertices[4];
			for (uint32_t i = 0; i < 4; i++)
			{
				vertices[i].Position = transform * s_Data->QuadVertexPositions[i];
				vertices[i].Color = color;
				vertices[i].TexCoord = s_Data->QuadVertexTexCoords[i];
				vertices[i].TexIndex = 0.0f;
			}

			s_Data->QuadVertexBuffer->AddData(&vertices, sizeof(Vertex) * 4);
			s_Data->QuadIndexCount += 6;
		});
	}

	void Renderer::DrawQuad(const glm::vec2& position, Ref<Texture> texture, const glm::vec4& tintColor)
	{
		DrawQuad({ position.x, position.y, 0.0f }, texture, tintColor);
	}

	void Renderer::DrawQuad(const glm::vec3& position, Ref<Texture> texture, const glm::vec4& tintColor)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
		DrawQuad(transform, texture, tintColor);
	}

	void Renderer::DrawQuad(const glm::mat4& transform, Ref<Texture> texture, const glm::vec4& tintColor)
	{
		Renderer::SubmitCommand([=]()
		{
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

			Vertex vertices[4];
			for (uint32_t i = 0; i < 4; i++)
			{
				vertices[i].Position = transform * s_Data->QuadVertexPositions[i];
				vertices[i].Color = tintColor;
				vertices[i].TexCoord = s_Data->QuadVertexTexCoords[i];
				vertices[i].TexIndex = textureIndex;
			}

			s_Data->QuadVertexBuffer->AddData(&vertices, sizeof(Vertex) * 4);
			s_Data->QuadIndexCount += 6;
		});
	}
}
