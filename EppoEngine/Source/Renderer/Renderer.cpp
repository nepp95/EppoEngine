#include "pch.h"
#include "Renderer.h"

#include "Asset/AssetManager.h"
#include "Renderer/Buffer/IndexBuffer.h"
#include "Renderer/Buffer/VertexBuffer.h"
#include "Renderer/Descriptor/DescriptorBuilder.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RendererContext.h"
#include "Renderer/Vertex.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

namespace Eppo
{
	struct RendererData
	{
		static const uint32_t MaxQuads = 20000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32;

		Ref<RenderCommandBuffer> CommandBuffer;
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

		// Geometry
		Ref<Pipeline> GeometryPipeline;
		std::map<AssetHandle, glm::mat4> GeometryTransformData;
		std::map<AssetHandle, Ref<Mesh>> GeometryDrawList;

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

		FramebufferSpecification framebufferSpec;
		framebufferSpec.Attachments = { ImageFormat::RGBA8, ImageFormat::Depth };
		framebufferSpec.Width = swapchain->GetWidth();
		framebufferSpec.Height = swapchain->GetHeight();
		framebufferSpec.Clear = true;
		framebufferSpec.ClearColor = { 0.4f, 0.4f, 0.4f, 1.0f };

		PipelineSpecification geometryPipelineSpec;
		geometryPipelineSpec.Framebuffer = CreateRef<Framebuffer>(framebufferSpec);
		geometryPipelineSpec.Shader = s_Data->ShaderLibrary->Get("geometry");
		geometryPipelineSpec.Layout = {
			{ ShaderDataType::Float3, "inPosition" },
			{ ShaderDataType::Float3, "inNormal" },
			{ ShaderDataType::Float2, "inTexCoord" },
		};
		geometryPipelineSpec.DepthTesting = true;
		geometryPipelineSpec.PushConstants = {
			{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) },
			{ VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::vec3) + sizeof(float) }
		};

		s_Data->GeometryPipeline = CreateRef<Pipeline>(geometryPipelineSpec);

		// Textures
		uint32_t whiteTextureData = 0xffffffff;
		s_Data->WhiteTexture = CreateRef<Texture>(1, 1, ImageFormat::RGBA8, &whiteTextureData);
		s_Data->TextureSlots[0] = s_Data->WhiteTexture;

		// Camera
		s_Data->CameraUniformBuffer = CreateRef<UniformBuffer>(geometryPipelineSpec.Shader, sizeof(RendererData::CameraBuffer));
	}

	void Renderer::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("Renderer::Shutdown");

		s_Data->DescriptorCache->Shutdown();
		s_Data->DescriptorAllocator->Shutdown();

		delete s_Data;
	}

	void Renderer::BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Framebuffer> framebuffer, VkSubpassContents flags)
	{
		SubmitCommand([renderCommandBuffer, framebuffer, flags]()
		{
			EPPO_PROFILE_FUNCTION("Renderer::BeginRenderPass");

			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();

			VkCommandBuffer commandBuffer = renderCommandBuffer->GetCurrentCommandBuffer();

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderArea.offset = { 0, 0 };

			if (!framebuffer)
			{
				renderPassInfo.renderPass = swapchain->GetRenderPass();
				renderPassInfo.framebuffer = swapchain->GetCurrentFramebuffer();
				renderPassInfo.renderArea.extent = swapchain->GetExtent();
			}
			else
			{
				renderPassInfo.renderPass = framebuffer->GetRenderPass();
				renderPassInfo.framebuffer = framebuffer->GetFramebuffer();
				renderPassInfo.renderArea.extent = framebuffer->GetExtent();
				renderPassInfo.clearValueCount = (uint32_t)framebuffer->GetClearValues().size();
				renderPassInfo.pClearValues = framebuffer->GetClearValues().data();
			}

			vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, flags);
		});
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

		s_Data->CommandBuffer->Begin();

		// Quads
		BeginRenderPass(s_Data->CommandBuffer, s_Data->GeometryPipeline->GetSpecification().Framebuffer);
		
		uint32_t dataSize = (uint32_t)((uint8_t*)s_Data->QuadVertexBufferPtr - (uint8_t*)s_Data->QuadVertexBufferBase);
		if (dataSize)
		{
			SubmitCommand([dataSize]()
			{
				s_Data->QuadVertexBuffer->SetData(s_Data->QuadVertexBufferBase, dataSize);

				Ref<RendererContext> context = RendererContext::Get();
				Ref<Swapchain> swapchain = context->GetSwapchain();

				VkCommandBuffer commandBuffer = s_Data->CommandBuffer->GetCurrentCommandBuffer();

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

		// Mesh
		for (auto& [handle, mesh] : s_Data->GeometryDrawList)
			Renderer::DrawGeometry(mesh);

		EndRenderPass(s_Data->CommandBuffer);

		s_Data->CommandBuffer->End();
		s_Data->CommandBuffer->Submit();

		s_Data->GeometryDrawList.clear();
		s_Data->GeometryTransformData.clear();

		UpdateStatistics();
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

	void Renderer::UpdateStatistics()
	{

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

	void Renderer::DrawQuad(const glm::vec2& position, SpriteComponent& sc, int entityId)
	{
		EPPO_PROFILE_FUNCTION("Renderer::DrawQuad");

		DrawQuad({ position.x, position.y, 0.0f }, sc, entityId);
	}

	void Renderer::DrawQuad(const glm::vec3& position, SpriteComponent& sc, int entityId)
	{
		EPPO_PROFILE_FUNCTION("Renderer::DrawQuad");

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
		DrawQuad(transform, sc, entityId);
	}

	void Renderer::DrawQuad(const glm::mat4& transform, SpriteComponent& sc, int entityId)
	{
		EPPO_PROFILE_FUNCTION("Renderer::DrawQuad");

		if (sc.TextureHandle)
		{
			Ref<Texture> texture = AssetManager::Get().GetAsset<Texture>(sc.TextureHandle);
			if (texture)
			{
				DrawQuad(transform, texture, sc.Color);
				return;
			}
		}
		
		DrawQuad(transform, sc.Color);
	}

	void Renderer::SubmitGeometry(const glm::vec2& position, MeshComponent& mc)
	{
		EPPO_PROFILE_FUNCTION("Renderer::SubmitGeometry");

		SubmitGeometry({ position.x, position.y, 0.0f }, mc);
	}

	void Renderer::SubmitGeometry(const glm::vec3& position, MeshComponent& mc)
	{
		EPPO_PROFILE_FUNCTION("Renderer::SubmitGeometry");

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
		SubmitGeometry(transform, mc);
	}

	void Renderer::SubmitGeometry(const glm::mat4& transform, MeshComponent& mc)
	{
		EPPO_PROFILE_FUNCTION("Renderer::SubmitGeometry");

		AssetManager& assetManager = AssetManager::Get();
		Ref<Mesh> mesh = assetManager.GetAsset<Mesh>(mc.MeshHandle);

		s_Data->GeometryDrawList[mesh->Handle] = mesh;
		s_Data->GeometryTransformData[mesh->Handle] = transform;
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
				s_Data->GeometryPipeline->GetPipelineLayout(),
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

	void Renderer::DrawGeometry(Ref<Mesh> mesh)
	{
		SubmitCommand([mesh]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();

			VkCommandBuffer commandBuffer = s_Data->CommandBuffer->GetCurrentCommandBuffer();

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_Data->GeometryPipeline->GetPipeline());

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

			VkDescriptorSet descriptorSet = s_Data->CameraUniformBuffer->GetCurrentDescriptorSet();
			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				s_Data->GeometryPipeline->GetPipelineLayout(),
				1,
				1,
				&descriptorSet,
				0,
				nullptr
			);

			const auto& submeshes = mesh->GetSubmeshes();
			for (const auto& submesh : submeshes)
			{
				VkBuffer vbo[] = { submesh.GetVertexBuffer()->GetBuffer() };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vbo, offsets);
				vkCmdBindIndexBuffer(commandBuffer, submesh.GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdPushConstants(commandBuffer, s_Data->GeometryPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &s_Data->GeometryTransformData[mesh->Handle]);
				vkCmdPushConstants(commandBuffer, s_Data->GeometryPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::vec3) + sizeof(float), &mesh->GetMaterial(submesh.GetMaterialIndex()).DiffuseColor);

				// Draw
				vkCmdDrawIndexed(commandBuffer, submesh.GetIndexBuffer()->GetIndexCount(), 1, 0, 0, 0);
			}
		});
	}
}
