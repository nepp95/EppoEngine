#pragma once

#include "Renderer/Material.h"
#include "Renderer/RenderCommandQueue.h"
#include "Renderer/Texture.h"

#include <glm/glm.hpp>

typedef struct VkDescriptorSet_T* VkDescriptorSet;
typedef struct VkDescriptorSetLayout_T* VkDescriptorSetLayout;

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

		// Scene
		static void BeginScene();
		static void EndScene();

		// Render commands
		static void ExecuteRenderCommands();
		static void SubmitCommand(RenderCommand command);

		// Descriptor Sets
		static VkDescriptorSet AllocateDescriptorSet(const VkDescriptorSetLayout& layout);
		static void UpdateDescriptorSet(Ref<Texture> texture, VkDescriptorSet descriptorSet, uint32_t arrayElement = 0);

		// Primitives
		static void DrawQuad(const glm::vec2& position, const glm::vec4& color = glm::vec4(1.0f));
		static void DrawQuad(const glm::vec3& position, const glm::vec4& color = glm::vec4(1.0f));
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));

		static void DrawQuad(const glm::vec2& position, Ref<Texture> texture, const glm::vec4& tintColor = glm::vec4(1.0f));
		static void DrawQuad(const glm::vec3& position, Ref<Texture> texture, const glm::vec4& tintColor = glm::vec4(1.0f));
		static void DrawQuad(const glm::mat4& transform, Ref<Texture> texture, const glm::vec4& tintColor = glm::vec4(1.0f));
	};
}
