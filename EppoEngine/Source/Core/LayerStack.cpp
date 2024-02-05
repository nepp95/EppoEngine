#include "pch.h"
#include "LayerStack.h"

namespace Eppo
{
	void LayerStack::PushLayer(Ref<Layer> layer)
	{
		EPPO_PROFILE_FUNCTION("LayerStack::PushLayer");

		m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
		m_LayerInsertIndex++;

		layer->OnAttach();
	}

	void LayerStack::PopLayer(Ref<Layer> layer)
	{
		EPPO_PROFILE_FUNCTION("LayerStack::PopLayer");

		auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);

		if (it != m_Layers.begin() + m_LayerInsertIndex)
		{
			layer->OnDetach();
			m_Layers.erase(it);
			m_LayerInsertIndex--;
		}
	}

	void LayerStack::PushOverlay(Ref<Layer> layer)
	{
		EPPO_PROFILE_FUNCTION("LayerStack::PushOverlay");

		m_Layers.emplace_back(layer);
		layer->OnAttach();
	}

	void LayerStack::PopOverlay(Ref<Layer> layer)
	{
		EPPO_PROFILE_FUNCTION("LayerStack::PopOverlay");

		auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);

		if (it != m_Layers.end())
		{
			layer->OnDetach();
			m_Layers.erase(it);
		}
	}
}
