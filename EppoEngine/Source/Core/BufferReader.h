#pragma once

#include "Core/Buffer.h"

namespace Eppo
{
	class BufferReader
	{
	public:
		explicit BufferReader(const Buffer& buffer);

		template<typename T>
		consteval static auto IsReadableType() -> bool
		{
			return std::is_trivially_copyable_v<T>;
		}

		template<typename T>
			requires(IsReadableType<T>())
		auto Read(T& value) -> bool
		{
			return ReadBytes(&value, sizeof(T));
		}

		auto ReadBytes(void* data, uint64_t size) -> bool;
		auto ReadString() -> std::string;
		auto ReadSubReader(uint64_t size) -> BufferReader;

		[[nodiscard]] auto IsValid() const -> bool { return m_IsValid; }
		[[nodiscard]] auto GetOffset() const -> uint64_t { return m_Offset; }
		[[nodiscard]] auto GetRemaining() const -> uint64_t { return m_Buffer.Size - m_Offset; }

	private:
		BufferReader(const Buffer& buffer, bool isValid);
		auto CanRead(uint64_t size) -> bool;

	private:
		Buffer m_Buffer;
		uint64_t m_Offset = 0;
		bool m_IsValid = true;
	};
}
