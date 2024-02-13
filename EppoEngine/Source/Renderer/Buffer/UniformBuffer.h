#pragma once

#include "Renderer/Shader.h"

namespace Eppo
{
	class UniformBuffer : public RefCounter
	{
	public:
		virtual ~UniformBuffer() {};

		virtual void SetData(void* data, uint32_t size) = 0;
		
		uint32_t GetBinding() const { return m_Binding; }

		static Ref<UniformBuffer> Create(uint32_t size, uint32_t binding);

	protected:
		UniformBuffer(uint32_t size, uint32_t binding)
			: m_Size(size), m_Binding(binding)
		{}

	protected:
		uint32_t m_Size;
		uint32_t m_Binding;
	};
}
