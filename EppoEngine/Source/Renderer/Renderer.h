#pragma once

#include "Renderer/RenderCommandQueue.h"

#include <glm/glm.hpp>

namespace Eppo
{
	class Renderer
	{
	public:
		static void Init();
		static void Shutdown();

		// Frames
		static void BeginFrame();
		static void EndFrame();

		// Scene
		static void BeginScene();
		static void EndScene();

		// Render commands
		static void ExecuteRenderCommands();
		static void SubmitCommand(RenderCommand command);

		// Primitives
		static void DrawQuad(const glm::vec2& position, const glm::vec4& color);
		static void DrawQuad(const glm::vec3& position, const glm::vec4& color);
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
	};
}
