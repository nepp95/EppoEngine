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

		// Shaders
		Scope<ShaderLibrary> ShaderLibrary;
	};

	static RendererData* s_Data;

	void Renderer::Init()
	{
		EPPO_PROFILE_FUNCTION("Renderer::Init");

		s_Data = new RendererData();

		// Load shaders
		s_Data->ShaderLibrary = CreateScope<ShaderLibrary>();
		//s_Data->ShaderLibrary->Load("Resources/Shaders/test.glsl");
		s_Data->ShaderLibrary->Load("Resources/Shaders/geometry.glsl");
		s_Data->ShaderLibrary->Load("Resources/Shaders/shadow.glsl");
	}

	void Renderer::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("Renderer::Shutdown");

		delete s_Data;
	}

	void Renderer::BeginScene(const EditorCamera& editorCamera)
	{
		
	}

	void Renderer::EndScene()
	{
		
	}

	/*void Renderer::BeginRenderPass(const Ref<RenderCommandBuffer>& renderCommandBuffer, const Ref<Pipeline>& pipeline)
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		uint32_t frameIndex = GetCurrentFrameIndex();
		vkResetDescriptorPool(device, s_Data->DescriptorPools[frameIndex], 0);

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
	}*/

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

	void Renderer::Clear()
	{
		SubmitCommand([]()
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		});
	}

	Ref<Shader> Renderer::GetShader(const std::string& name)
	{
		return s_Data->ShaderLibrary->Get(name);
	}

	void Renderer::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Mesh> mesh)
	{
		SubmitCommand([renderCommandBuffer, mesh]()
		{
			for (const auto& submesh : mesh->GetSubmeshes())
			{
				// Bind shader
				s_Data->ShaderLibrary->Get("geometry")->Bind();

				// Bind vertex array
				Ref<VertexArray> vao = submesh.GetVertexArray();
				vao->Bind();

				// Draw
				glDrawElements(GL_TRIANGLES, vao->GetIndexCount(), GL_UNSIGNED_INT, nullptr);
			}
		});
	}
}
