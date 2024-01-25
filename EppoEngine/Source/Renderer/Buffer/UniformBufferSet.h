#pragma once

#include "Renderer/Buffer/UniformBuffer.h"

#include <map>

namespace Eppo
{
	class UniformBufferSet : public RefCounter
	{
	public:
		UniformBufferSet() = default;
		~UniformBufferSet() = default;

		void Create(const Ref<UniformBuffer> uniformBuffer);

		Ref<UniformBuffer> Get(uint32_t frame, uint32_t set, uint32_t binding);
		void Set(const Ref<UniformBuffer>& uniformBuffer, uint32_t frame, uint32_t set);

	private:
		// Frame --> Set --> Binding --> Uniform buffer
		std::map<uint32_t, std::map<uint32_t, std::map<uint32_t, Ref<UniformBuffer>>>> m_UniformBufferSet; 
	};
}
