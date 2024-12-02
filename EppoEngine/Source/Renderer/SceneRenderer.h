#pragma once

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Image.h"
#include "Renderer/DrawCommand.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Eppo
{
	class Scene;
	using EntityHandle = entt::entity;

	// TODO: Move to renderer
	struct RenderSpecification
	{
		uint32_t Width = 0;
		uint32_t Height = 0;

		bool DebugRendering = false;
	};

	struct RenderStatistics
	{
		uint32_t DrawCalls = 0;
		uint32_t Meshes = 0;
		uint32_t Submeshes = 0;
		uint32_t MeshInstances = 0;
	};

	class SceneRenderer
	{
	public:
		virtual ~SceneRenderer() = default;

		virtual void RenderGui() = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual void BeginScene(const EditorCamera& editorCamera) = 0;
		virtual void BeginScene(const Camera& camera, const glm::mat4& transform) = 0;
		virtual void EndScene() = 0;

		virtual void SubmitDrawCommand(EntityType type, Ref<DrawCommand> drawCommand) = 0;
		virtual Ref<Image> GetFinalImage() = 0;

		static Ref<SceneRenderer> Create(Ref<Scene> scene, const RenderSpecification& renderSpec);
	};
}
