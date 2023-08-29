#pragma once

#include <EppoEngine.h>

namespace Eppo
{
	class Panel
	{
	public:
		virtual ~Panel() = default;

		virtual void RenderGui() = 0;
		
		virtual void SetSceneContext(const Ref<Scene>& scene) {}
		virtual void SetSelectedEntity(Entity entity) {}
	};
}
