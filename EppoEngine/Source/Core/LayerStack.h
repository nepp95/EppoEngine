#pragma once

#include "Core/Layer.h"

#include <vector>

namespace Eppo
{
	class LayerStack
	{
	public:
		LayerStack() = default;

		void PushLayer(Ref<Layer> layer);
		void PopLayer(Ref<Layer> layer);

		void PushOverlay(Ref<Layer> layer);
		void PopOverlay(Ref<Layer> layer);

		std::vector<Ref<Layer>>::iterator begin() { return m_Layers.begin(); }
		std::vector<Ref<Layer>>::iterator end() { return m_Layers.end(); }
		std::vector<Ref<Layer>>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
		std::vector<Ref<Layer>>::reverse_iterator rend() { return m_Layers.rend(); }

		std::vector<Ref<Layer>>::const_iterator begin() const { return m_Layers.end(); }
		std::vector<Ref<Layer>>::const_iterator end() const { return m_Layers.end(); }
		std::vector<Ref<Layer>>::const_reverse_iterator rbegin() const { return m_Layers.rbegin(); }
		std::vector<Ref<Layer>>::const_reverse_iterator rend() const { return m_Layers.rend(); }

	private:
		std::vector<Ref<Layer>> m_Layers;
		uint32_t m_LayerInsertIndex = 0;
	};
}
