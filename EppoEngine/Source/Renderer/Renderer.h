#pragma once

#include "Renderer/Buffer/UniformBuffer.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Descriptor/DescriptorBuilder.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderCommandQueue.h"
#include "Renderer/ShaderLibrary.h"
#include "Renderer/Texture.h"
#include "Scene/Components.h"

#include <glm/glm.hpp>

namespace Eppo
{
	class Renderer
	{
	public:
		static void Init();
		static void Shutdown();

		// Render pass
		static void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Framebuffer> framebuffer, VkSubpassContents flags = VK_SUBPASS_CONTENTS_INLINE);
		static void EndRenderPass(const Ref<RenderCommandBuffer>& renderCommandBuffer);

		static void BeginRenderPass(const Ref<RenderCommandBuffer>& renderCommandBuffer, const Ref<Pipeline>& pipeline);

		// Batch
		static void StartBatch();
		static void NextBatch();
		static void Flush();

		// Scene
		static void BeginScene(const EditorCamera& camera);
		static void EndScene();

		// Render commands
		static void ExecuteRenderCommands();
		static void SubmitCommand(RenderCommand command);

		// Shaders
		static Ref<Shader> GetShader(const std::string& name);

		// Descriptor Sets
		static Ref<DescriptorAllocator> GetDescriptorAllocator();
		static Ref<DescriptorLayoutCache> GetDescriptorLayoutCache();

		// Statistics
		static void UpdateStatistics();

		// Primitives
		static void DrawQuad(const glm::vec2& position, const glm::vec4& color = glm::vec4(1.0f));
		static void DrawQuad(const glm::vec3& position, const glm::vec4& color = glm::vec4(1.0f));
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));

		static void DrawQuad(const glm::vec2& position, Ref<Texture> texture, const glm::vec4& tintColor = glm::vec4(1.0f));
		static void DrawQuad(const glm::vec3& position, Ref<Texture> texture, const glm::vec4& tintColor = glm::vec4(1.0f));
		static void DrawQuad(const glm::mat4& transform, Ref<Texture> texture, const glm::vec4& tintColor = glm::vec4(1.0f));

		static void DrawQuad(const glm::vec2& position, SpriteComponent& sc, int entityId);
		static void DrawQuad(const glm::vec3& position, SpriteComponent& sc, int entityId);
		static void DrawQuad(const glm::mat4& transform, SpriteComponent& sc, int entityId);

		// Geometry
		static void SubmitGeometry(const glm::vec2& position, MeshComponent& mc);
		static void SubmitGeometry(const glm::vec3& position, MeshComponent& mc);
		static void SubmitGeometry(const glm::mat4& transform, MeshComponent& mc);

		static void RenderGeometry(const Ref<RenderCommandBuffer>& renderCommandBuffer, const Ref<Pipeline>& pipeline, const Ref<UniformBuffer>& cameraBuffer, const Ref<Mesh>& mesh, const glm::mat4& transform);

		static void DrawGeometry(Ref<Mesh> mesh);

	private:
		RenderCommandQueue m_CommandQueue;

		// Textures
		Ref<Texture> m_WhiteTexture;
		std::array<Ref<Texture>, 32> m_TextureSlots;
		uint32_t m_TextureSlotIndex = 1;
	};
}
