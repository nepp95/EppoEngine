#include "pch.h"
#include "Renderer/StorageBuffer.h"

#include "Renderer/DeviceManager.h"

namespace Eppo
{
	StorageBuffer::StorageBuffer(uint32_t structStride, uint64_t initialSize, const std::string& debugName)
		: m_Stride(structStride), m_DebugName(debugName)
	{
		EP_PROFILE_FN("StorageBuffer::StorageBuffer");

		m_Size = initialSize >= m_Stride ? initialSize : m_Stride;
		CreateBuffer();
	}

	auto StorageBuffer::SetData(const void* data, uint64_t size, uint64_t offset) -> void
	{
		EP_PROFILE_FN("StorageBuffer::SetData");

		// Nothing to upload (e.g. an empty scene with no instance transforms).
		// This is an expected per-frame case, so it is a quiet no-op rather than
		// a logged error. Writing here would read from a null/short source.
		if (size == 0 || !data)
			return;

		const auto device = DeviceManager::Get()->GetDevice();
		const auto cmd = device->createCommandList();

		if (size > m_Size)
		{
			Log::Warn("Setting data on a buffer that isn't big enough, recreating buffer...");
			m_Size = size;
			CreateBuffer();
		}

		// Write exactly the bytes provided, not the whole buffer: the buffer may
		// be larger than the current payload (it only ever grows), so writing
		// m_Size would over-read the source data.
		cmd->open();
		cmd->writeBuffer(m_Buffer, data, size, offset);
		cmd->close();

		device->executeCommandList(cmd);
	}

	auto StorageBuffer::CreateBuffer() -> void
	{
		EP_PROFILE_FN("StorageBuffer::CreateBuffer");

		const auto device = DeviceManager::Get()->GetDevice();

		nvrhi::BufferDesc bufferDesc{
			.byteSize = m_Size,
			.structStride = m_Stride,
			.debugName = m_DebugName,
			.initialState = nvrhi::ResourceStates::ShaderResource,
			.keepInitialState = true,
		};

		m_Buffer = device->createBuffer(bufferDesc);
	}
}