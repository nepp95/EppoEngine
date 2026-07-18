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

	auto UniformBuffer::SetData(const nvrhi::CommandListHandle& cmdList, const void* data, const uint64_t size, const uint64_t offset) -> void
	{
		EP_PROFILE_FN("UniformBuffer::SetData");

		if (size > m_Size)
		{
			Log::Warn("Setting data on a buffer that isn't big enough, recreating buffer...");
			m_Size = size;
			CreateBuffer();
		}

		cmdList->writeBuffer(m_Buffer, data, size, offset);
	}

	auto UniformBuffer::SetData(const void* data, const uint64_t size, const uint64_t offset) -> void
	{
		EP_PROFILE_FN("UniformBuffer::SetData");

		const auto device = DeviceManager::Get()->GetDevice();
		const auto cmd = device->createCommandList();
		cmd->open();
		SetData(cmd, data, size, offset);
		cmd->close();

		device->executeCommandList(cmd);
	}

	auto UniformBuffer::CreateBuffer() -> void
	{
		EP_PROFILE_FN("UniformBuffer::CreateBuffer");

		const auto device = DeviceManager::Get()->GetDevice();

		const nvrhi::BufferDesc bufferDesc{
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
