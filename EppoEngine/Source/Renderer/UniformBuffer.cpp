#include "pch.h"
#include "Renderer/UniformBuffer.h"

#include "Renderer/DeviceManager.h"

namespace Eppo
{
	UniformBuffer::UniformBuffer(const uint64_t size, std::string debugName)
		: m_Size(size), m_DebugName(std::move(debugName))
	{
		EP_PROFILE_FN("UniformBuffer::UniformBuffer");

		CreateBuffer();
	}

	auto UniformBuffer::SetData(const void* data, uint64_t size, uint64_t offset /*= 0*/) -> void
	{
		EP_PROFILE_FN("UniformBuffer::SetData");

		const auto device = DeviceManager::Get()->GetDevice();
		const auto cmd = device->createCommandList();

		if (size > m_Size)
		{
			Log::Warn("Setting data on a buffer that isn't big enough, recreating buffer...");
			m_Size = size;
			CreateBuffer();
		}

		cmd->open();
		cmd->writeBuffer(m_Buffer, data, m_Size);
		cmd->close();

		device->executeCommandList(cmd);
	}

	auto UniformBuffer::CreateBuffer() -> void
	{
		EP_PROFILE_FN("UniformBuffer::CreateBuffer");

		const auto device = DeviceManager::Get()->GetDevice();

		nvrhi::BufferDesc bufferDesc{
			.byteSize = m_Size,
			.debugName = m_DebugName,
			.isConstantBuffer = true,
			.initialState = nvrhi::ResourceStates::ConstantBuffer,
			.keepInitialState = true,
			.cpuAccess = nvrhi::CpuAccessMode::Write,
		};

		m_Buffer = device->createBuffer(bufferDesc);
	}
}