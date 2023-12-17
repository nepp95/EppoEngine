#include "pch.h"
#include "UniformBufferSet.h"

namespace Eppo
{
	Ref<UniformBuffer> UniformBufferSet::Get(uint32_t frame, uint32_t set, uint32_t binding)
	{
		EPPO_ASSERT(m_UniformBufferSet.find(frame) != m_UniformBufferSet.end());
		EPPO_ASSERT(m_UniformBufferSet.at(frame).find(set) != m_UniformBufferSet.at(frame).end());
		EPPO_ASSERT(m_UniformBufferSet.at(frame).at(set).find(binding) != m_UniformBufferSet.at(frame).at(set).end());

		return m_UniformBufferSet.at(frame).at(set).at(binding);
	}

	void UniformBufferSet::Set(const Ref<UniformBuffer>& uniformBuffer, uint32_t frame, uint32_t set)
	{
		m_UniformBufferSet[frame][set][uniformBuffer->GetBinding()] = uniformBuffer;
	}
}
