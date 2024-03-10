#include "pch.h"
#include "OpenGLRenderer.h"

#include "Core/Application.h"
#include "Platform/OpenGL/OpenGL.h"
#include "Platform/OpenGL/OpenGLIndexBuffer.h"
#include "Platform/OpenGL/OpenGLPipeline.h"
#include "Platform/OpenGL/OpenGLRenderCommandBuffer.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include "Platform/OpenGL/OpenGLVertexBuffer.h"
#include "Renderer/RenderCommandQueue.h"

namespace Eppo
{
    struct RendererData
    {
		Scope<RenderCommandQueue> CommandQueue;
		Ref<RenderCommandBuffer> CommandBuffer;
    };

    static RendererData* s_Data = nullptr;

    OpenGLRenderer::~OpenGLRenderer()
    {
        EPPO_PROFILE_FUNCTION("OpenGLRenderer::~OpenGLRenderer");

        Shutdown();
    }

    void OpenGLRenderer::Init()
    {
		#ifdef EPPO_DEBUG
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback(DebugCallback, nullptr);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
		#endif

		s_Data = new RendererData();

		//glEnable(GL_BLEND);
		//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		//glEnable(GL_DEPTH_TEST);

		s_Data->CommandBuffer = RenderCommandBuffer::Create();
		s_Data->CommandQueue = CreateScope<RenderCommandQueue>();
    }

    void OpenGLRenderer::Shutdown()
    {
		delete s_Data;
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
		s_Data->CommandQueue->Execute();
    }

    void OpenGLRenderer::SubmitCommand(RenderCommand command)
    {
		s_Data->CommandQueue->AddCommand(command);
    }

    void OpenGLRenderer::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<UniformBufferSet> uniformBufferSet, Ref<Mesh> mesh, const glm::mat4 &transform)
    {
		SubmitCommand([renderCommandBuffer, pipeline, uniformBufferSet, mesh, transform]
		{
			auto& app = Application::Get();

			// TODO: Do we need this?
			//glViewport(0, 0, app.GetWindow().GetWidth(), app.GetWindow().GetHeight());
			
			Ref<OpenGLPipeline> OGLPipeline = pipeline.As<OpenGLPipeline>();

			for (const auto& submesh : mesh->GetSubmeshes())
			{
				OGLPipeline->AddVertexBuffer(submesh.GetVertexBuffer());

				// Bind vertex array
				OGLPipeline->GetSpecification().Shader.As<OpenGLShader>()->Bind();
				OGLPipeline->Bind();

				Ref<OpenGLIndexBuffer> ib = submesh.GetIndexBuffer().As<OpenGLIndexBuffer>();
				ib->Bind();
				// do something with uniforms

				// draw indexed
				glDrawElements(GL_TRIANGLES, ib->GetIndexCount(), GL_UNSIGNED_INT, nullptr);
			}
		});
    }
}
