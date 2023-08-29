#pragma once

#include "Panel/Panel.h"

namespace Eppo
{
	class SceneHierarchyPanel : public Panel
	{
	public:
		SceneHierarchyPanel() = default;

		void RenderGui() override;

		void SetSceneContext(const Ref<Scene>& scene) override;
		void SetSelectedEntity(Entity entity) override;

	private:
		void DrawEntityNode(Entity entity);

	private:
		Ref<Scene> m_SceneContext;
		Entity m_SelectedEntity;
	};
}
