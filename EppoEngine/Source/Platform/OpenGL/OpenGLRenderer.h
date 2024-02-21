#pragma once

#include "Renderer/RendererAPI.h"

namespace Eppo
{
    class OpenGLRenderer : public RendererAPI
    {
    public:
        OpenGLRenderer() = default;
        ~OpenGLRenderer();

        void Init() override;
        void Shutdown() override;

        // Frame index
		uint32_t GetCurrentFrameIndex() const override;

		// Render pass
		void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline) override;
		void EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer) override;

		// Render commands
		void ExecuteRenderCommands() override;
		void SubmitCommand(RenderCommand command) override;

		// Geometry
		void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<Mesh> mesh, const glm::mat4& transform) override;
    };
}
