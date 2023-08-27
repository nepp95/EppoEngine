#pragma once

#include "Renderer/Descriptor/DescriptorBuilder.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Material.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderCommandQueue.h"
#include "Renderer/Texture.h"

#include <glm/glm.hpp>

namespace Eppo
{
	class Renderer
	{
	public:
		static void Init();
		static void Shutdown();

		// Frames
		static void BeginFrame();
		static void EndFrame();

		// Render pass
		static void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Framebuffer> framebuffer, VkSubpassContents flags = VK_SUBPASS_CONTENTS_INLINE);
		static void EndRenderPass();

		// Batch
		static void StartBatch();
		static void NextBatch();
		static void Flush();

		// Scene
		static void BeginScene(const EditorCamera& camera);
		static void EndScene();

		// Final image
		static Ref<Image> GetFinalImage();

		// Render commands
		static void ExecuteRenderCommands();
		static void SubmitCommand(RenderCommand command);

		// Descriptor Sets
		static Ref<DescriptorAllocator> GetDescriptorAllocator();
		static Ref<DescriptorLayoutCache> GetDescriptorLayoutCache();

		// Primitives
		static void DrawQuad(const glm::vec2& position, const glm::vec4& color = glm::vec4(1.0f));
		static void DrawQuad(const glm::vec3& position, const glm::vec4& color = glm::vec4(1.0f));
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));

		static void DrawQuad(const glm::vec2& position, Ref<Texture> texture, const glm::vec4& tintColor = glm::vec4(1.0f));
		static void DrawQuad(const glm::vec3& position, Ref<Texture> texture, const glm::vec4& tintColor = glm::vec4(1.0f));
		static void DrawQuad(const glm::mat4& transform, Ref<Texture> texture, const glm::vec4& tintColor = glm::vec4(1.0f));
	};
}
