#pragma once

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Material.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderCommandQueue.h"
#include "Renderer/Shader.h"
#include "Renderer/UniformBuffer.h"

namespace Eppo
{
	class Renderer
	{
	public:
		static void Init();
		static void Shutdown();

		// Scene management
		static void BeginScene(const EditorCamera& editorCamera);
		static void EndScene();

		// Render commands
		static void ExecuteRenderCommands();
		static void SubmitCommand(RenderCommand command);
		static void RT_Clear();

		// Shaders
		static Ref<Shader> GetShader(const std::string& name);

		// Geometry
		static void RT_RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<UniformBuffer> materialUB, Ref<Mesh> mesh);
	};
}
