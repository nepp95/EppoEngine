#pragma once

#include "Panels/Panel.h"

#include <EppoEngine.h>

namespace Eppo
{
	struct PanelData
	{
		Ref<Panel> Panel = nullptr;
		bool IsOpen = false;
	};

	class PanelManager
	{
	public:
		auto RenderGui() -> void;

		template<typename T>
			requires(std::derived_from<T, Panel>)
		auto AddPanel(const std::string& panelName, bool isOpen) -> void
		{
			PanelData data{
				.Panel = CreateRef<T>(),
				.IsOpen = isOpen,
			};

			data.Panel->SetPanelManager(this);
			m_PanelData[panelName] = data;
		}

		template<typename T>
			requires(std::derived_from<T, Panel>)
		auto GetPanel(const std::string& panelName) -> Ref<T>
		{
			auto it = m_PanelData.find(panelName);

			if (it != m_PanelData.end())
				return std::static_pointer_cast<T>(it->second.Panel);

			return nullptr;
		}

		auto HasPanel(const std::string& panelName) const -> bool { return m_PanelData.contains(panelName); }
		auto TogglePanel(const std::string& panelName) -> void;

		auto SetSceneContext(const Ref<Scene>& scene) -> void { m_SceneContext = scene; }
		auto GetSceneContext() const -> const Ref<Scene>& { return m_SceneContext; }
		
		auto SetSelectedEntity(Entity entity) -> void { m_SelectedEntity = entity; }
		auto GetSelectedEntity() const -> Entity { return m_SelectedEntity; }

	private:
		std::unordered_map<std::string, PanelData> m_PanelData;

		Ref<Scene> m_SceneContext = nullptr;
		Entity m_SelectedEntity;
	};
}