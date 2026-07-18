#pragma once

#include <nvrhi/nvrhi.h>

namespace Eppo
{
	class StorageBuffer
	{
	public:
		StorageBuffer(uint32_t structStride, uint64_t initialSize = 0, std::string debugName = "StorageBuffer");

		auto SetData(const nvrhi::CommandListHandle& cmdList, const void* data, uint64_t size, uint64_t offset = 0) -> void;
		auto SetData(const void* data, uint64_t size, uint64_t offset = 0) -> void;

		[[nodiscard]] auto GetBuffer() const -> nvrhi::BufferHandle { return m_Buffer; }
		[[nodiscard]] constexpr auto GetSize() const -> uint64_t { return m_Size; };
		[[nodiscard]] constexpr auto GetStride() const -> uint32_t { return m_Stride; };

	private:
		auto CreateBuffer() -> void;

	private:
		nvrhi::BufferHandle m_Buffer = nullptr;
		uint64_t m_Size = 0;
		uint32_t m_Stride = 0;
		std::string m_DebugName;
	};
}
