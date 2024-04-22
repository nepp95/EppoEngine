#pragma once

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderCommandQueue.h"
#include "Renderer/Shader.h"
#include "Renderer/UniformBuffer.h"

typedef unsigned int GLenum;

namespace Eppo
{
	enum class FaceCulling
	{
		FRONT_LEFT = 1024,
		FRONT_RIGHT,
		BACK_LEFT,
		BACK_RIGHT,
		FRONT,
		BACK,
		LEFT,
		RIGHT,
		FRONT_AND_BACK
	};

	class Renderer
	{
	public:
		static void Init();
		static void Shutdown();

		// Render commands
		static void ExecuteRenderCommands();
		static void SubmitCommand(RenderCommand command);
		static void Clear(bool color = true, bool depth = true);
		static void SetFaceCulling(FaceCulling face);

		// Shaders
		static Ref<Shader> GetShader(const std::string& name);

		// Geometry
		static void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Submesh> mesh);
		static Ref<Texture> GetWhiteTexture();
	};
}
