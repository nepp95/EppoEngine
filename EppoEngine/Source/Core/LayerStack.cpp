#include "pch.h"
#include "LayerStack.h"

namespace Eppo
{
	void LayerStack::PushLayer(Layer* layer)
	{
		EPPO_PROFILE_FUNCTION("LayerStack::PushLayer");

		m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
		m_LayerInsertIndex++;

		layer->OnAttach();
	}

	void LayerStack::PopLayer(Layer* layer)
	{
		EPPO_PROFILE_FUNCTION("LayerStack::PopLayer");

		if (const auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);
			it != m_Layers.begin() + m_LayerInsertIndex)
		{
			layer->OnDetach();
			m_Layers.erase(it);
			m_LayerInsertIndex--;
		}
	}

	void LayerStack::PushOverlay(Layer* layer)
	{
		EPPO_PROFILE_FUNCTION("LayerStack::PushOverlay");

		m_Layers.emplace_back(layer);
		layer->OnAttach();
	}

	void LayerStack::PopOverlay(Layer* layer)
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
