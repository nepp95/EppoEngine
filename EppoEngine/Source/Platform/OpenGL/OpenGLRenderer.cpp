#include "pch.h"
#include "OpenGLRenderer.h"

namespace Eppo
{
    struct RendererData
    {
    };

    static RendererData* s_Data = nullptr;

    OpenGLRenderer::~OpenGLRenderer()
    {
        EPPO_PROFILE_FUNCTION("OpenGLRenderer::~OpenGLRenderer");

        Shutdown();

        delete s_Data;
    }

    void OpenGLRenderer::Init()
    {

    }

    void OpenGLRenderer::Shutdown()
    {
        
    }

    uint32_t OpenGLRenderer::GetCurrentFrameIndex() const
    {
        return 0;
    }

    void OpenGLRenderer::BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline)
    {
    }

    void OpenGLRenderer::EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer)
    {
    }

    void OpenGLRenderer::ExecuteRenderCommands()
    {
    }

    void OpenGLRenderer::SubmitCommand(RenderCommand command)
    {
    }

    Ref<Shader> OpenGLRenderer::GetShader(const std::string &name)
    {
        return Ref<Shader>();
    }

    void OpenGLRenderer::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<Mesh> mesh, const glm::mat4 &transform)
    {
    }
}
