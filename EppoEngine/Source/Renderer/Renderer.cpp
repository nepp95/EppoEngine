#include "pch.h"
#include "Renderer.h"

#include "Renderer/IndexBuffer.h"
#include "Renderer/RendererContext.h"
#include "Renderer/ShaderLibrary.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Eppo
{
	struct RendererData
	{
		RenderCommandQueue CommandQueue;
		Scope<ShaderLibrary> ShaderLibrary;
	};

	static RendererData* s_Data;

	void Renderer::Init()
	{
		EPPO_PROFILE_FUNCTION("Renderer::Init");

		s_Data = new RendererData();

		// Load shaders
		s_Data->ShaderLibrary = CreateScope<ShaderLibrary>();
		s_Data->ShaderLibrary->Load("Resources/Shaders/geometry.glsl");
		s_Data->ShaderLibrary->Load("Resources/Shaders/predepth.glsl");
	}

	void Renderer::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("Renderer::Shutdown");

		delete s_Data;
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

	void Renderer::Clear(bool color, bool depth)
	{
		if (color && !depth)
			glClear(GL_COLOR_BUFFER_BIT);
		else if (!color && depth)
			glClear(GL_DEPTH_BUFFER_BIT);
		else if (color && depth)
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void Renderer::SetFaceCulling(FaceCulling face)
	{
		glCullFace((GLenum)face);
	}


	Ref<Shader> Renderer::GetShader(const std::string& name)
	{
		return s_Data->ShaderLibrary->Get(name);
	}

	void Renderer::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Submesh> mesh)
	{
		// Bind vertex array
		Ref<VertexArray> vao = mesh->GetVertexArray();
		vao->Bind();

		// Draw
		glDrawElements(GL_TRIANGLES, vao->GetIndexCount(), GL_UNSIGNED_INT, nullptr);
	}
}
