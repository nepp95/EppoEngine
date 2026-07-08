#pragma once

#include <nvrhi/nvrhi.h>

namespace Eppo
{
	class UniformBuffer
	{
	public:
        explicit UniformBuffer(uint64_t size, std::string debugName = "UniformBuffer");

		auto SetData(const void* data, uint64_t size, uint64_t offset = 0) -> void;

		[[nodiscard]] auto GetBuffer() const -> nvrhi::BufferHandle { return m_Buffer; }
		[[nodiscard]] constexpr auto GetSize() const -> uint64_t { return m_Size; };
		
	private:
		auto CreateBuffer() -> void;

	private:
		nvrhi::BufferHandle m_Buffer = nullptr;
		uint64_t m_Size = 0;
		std::string m_DebugName;
	};
}