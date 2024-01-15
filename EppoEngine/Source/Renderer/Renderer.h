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
#include "Renderer/Shader.h"

namespace Eppo
{
	class Renderer
	{
	public:
		virtual ~Renderer() {}

		virtual void Init() = 0;
		virtual void Shutdown() = 0;

		static void Init();
		static void Shutdown();

		// Frame index
		static uint32_t GetCurrentFrameIndex();

		// Render pass
		static void BeginRenderPass(const Ref<RenderCommandBuffer>& renderCommandBuffer, const Ref<Pipeline>& pipeline);
		static void EndRenderPass(const Ref<RenderCommandBuffer>& renderCommandBuffer);

		// Render commands
		static void ExecuteRenderCommands();
		static void SubmitCommand(RenderCommand command);

		// Shaders
		static Ref<Shader> GetShader(const std::string& name);

		// Descriptor Sets
		static Ref<DescriptorAllocator> GetDescriptorAllocator();
		static Ref<DescriptorLayoutCache> GetDescriptorLayoutCache();
		static VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo);

		// Geometry
		static void RenderGeometry(const Ref<RenderCommandBuffer>& renderCommandBuffer, const Ref<Pipeline>& pipeline, const Ref<UniformBuffer>& environmentUB, const Ref<UniformBuffer>& cameraUB, const Ref<Mesh>& mesh, const glm::mat4& transform);
	};
}
