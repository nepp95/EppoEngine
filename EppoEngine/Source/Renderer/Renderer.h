#pragma once

#include "Renderer/Buffer/UniformBuffer.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderCommandQueue.h"
#include "Renderer/Shader.h"

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

		// Shaders
		static Ref<Shader> GetShader(const std::string& name);

		// Geometry
		//static void RenderGeometry(const Ref<RenderCommandBuffer>& renderCommandBuffer, const Ref<Pipeline>& pipeline, const Ref<UniformBuffer>& environmentUB, const Ref<UniformBuffer>& cameraUB, const Ref<Mesh>& mesh, const glm::mat4& transform);
	};
}
