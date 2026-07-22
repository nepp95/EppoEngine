#pragma once

#include "Core/Buffer.h"

namespace Eppo
{
	class BufferWriter
	{
	public:
	    // Default constructor can be used to acquire data size
	    // Every write operation will simply increase the offset
		BufferWriter() = default;

		explicit BufferWriter(const Buffer& buffer);

		template<typename T>
		consteval static auto IsWritableType() -> bool
		{
			return std::is_trivially_copyable_v<T>;
		}

		template<typename T>
		    requires(IsWritableType<T>())
		auto Write(const T& value) -> bool
		{
			return WriteBytes(&value, sizeof(T));
		}

		auto WriteBytes(const void* data, uint64_t size) -> bool;
		auto WriteString(std::string_view value) -> bool;

		[[nodiscard]] auto IsValid() const -> bool { return m_IsValid; }
		[[nodiscard]] auto GetSize() const -> uint64_t { return m_Offset; }
		[[nodiscard]] auto GetOffset() const -> uint64_t { return m_Offset; }

	private:
		Buffer m_Buffer;
		uint64_t m_Offset = 0;
		bool m_IsSizing = true;
		bool m_IsValid = true;
	};
}
