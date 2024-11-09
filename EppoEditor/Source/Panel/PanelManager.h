#pragma once

#include "Panel/Panel.h"

#include <EppoEngine.h>

namespace Eppo
{
	struct PanelData
	{
		Ref<Panel> Panel;
		bool IsOpen = false;
	};

	class PanelManager
	{
	public:
		~PanelManager() = default;

		void RenderGui();

		Ref<Scene> GetSceneContext() { return m_SceneContext; }
		Entity GetSelectedEntity() const { return m_SelectedEntity; }

		void SetSceneContext(const Ref<Scene>& scene) { m_SceneContext = scene; }
		void SetSelectedEntity(Entity entity) { m_SelectedEntity = entity; }

		template<typename T, typename... Args>
		void AddPanel(const std::string& name, bool isOpen, Args&&... args)
		{
			static_assert(std::is_base_of_v<Panel, T>, "Class is not based on Panel!");

			if (HasPanel(name))
				return;

			PanelData panelData;
			panelData.Panel = CreateRef<T>(std::forward<Args>(args)...);
			panelData.IsOpen = isOpen;

			m_PanelData.insert({ name, panelData });
		}

		template<typename T>
		Ref<T> GetPanel(const std::string& name)
		{
			static_assert(std::is_base_of_v<Panel, T>, "Class is not based on Panel!");

			auto it = m_PanelData.find(name);
			if (it == m_PanelData.end())
				return nullptr;

			return it->second.Panel; // TODO: Might not work? dynamic cast to derived class
		}

		bool HasPanel(const std::string& name)
		{
			return m_PanelData.find(name) != m_PanelData.end();
		}

		static PanelManager& Get();

	private:
		PanelManager() = default;

	private:
		std::unordered_map<std::string, PanelData> m_PanelData;

		Ref<Scene> m_SceneContext;
		Entity m_SelectedEntity;
	};
}
