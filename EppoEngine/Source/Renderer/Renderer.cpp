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

	void Renderer::RT_Clear(bool color, bool depth)
	{
		SubmitCommand([color, depth]()
		{
			if (color && !depth)
				glClear(GL_COLOR_BUFFER_BIT);
			else if (!color && depth)
				glClear(GL_DEPTH_BUFFER_BIT);
			else if (color && depth)
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		});
	}

	void Renderer::RT_SetFaceCulling(FaceCulling face)
	{
		SubmitCommand([face]()
		{
			glCullFace((GLenum)face);
		});
	}

	Ref<Shader> Renderer::GetShader(const std::string& name)
	{
		return s_Data->ShaderLibrary->Get(name);
	}

	void Renderer::RT_RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<UniformBuffer> materialUB, Ref<Mesh> mesh)
	{
		SubmitCommand([renderCommandBuffer, materialUB, mesh]()
		{
			for (const auto& submesh : mesh->GetSubmeshes())
			{
				materialUB->RT_SetData((void*)&mesh->GetMaterial(submesh.GetMaterialIndex()).DiffuseColor);

				// Bind vertex array
				Ref<VertexArray> vao = submesh.GetVertexArray();
				vao->Bind();

				// Draw
				glDrawElements(GL_TRIANGLES, vao->GetIndexCount(), GL_UNSIGNED_INT, nullptr);
			}
		});
	}
}
