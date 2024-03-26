#pragma once

#include "Renderer/Shader.h"

namespace Eppo
{
	class UniformBuffer
	{
	public:
		UniformBuffer(uint32_t size, uint32_t binding);
		~UniformBuffer();

		void RT_SetData(void* data);
		void RT_SetData(void* data, uint32_t size);
		uint32_t GetBinding() const { return m_Binding; }

	private:
		uint32_t m_RendererID;
		uint32_t m_Size;
		uint32_t m_Binding;
	};
}
