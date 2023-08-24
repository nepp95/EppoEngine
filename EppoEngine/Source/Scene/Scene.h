#pragma once

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Image.h"

namespace Eppo
{
	class Scene
	{
	public:
		Scene() = default;
		~Scene() = default;

		void OnUpdate(float timestep);
		void Render(const EditorCamera& editorCamera);

		Ref<Image> GetFinalImage() const;

	private:

	};
}
