#include "pch.h"
#include "Core/BufferWriter.h"

namespace Eppo
{
	BufferWriter::BufferWriter(const Buffer& buffer)
		: m_Buffer(buffer), m_IsSizing(false)
	{}

	auto BufferWriter::WriteBytes(const void* data, const uint64_t size) -> bool
	{
		if (!m_IsValid)
			return false;

		if (size > std::numeric_limits<uint64_t>::max() - m_Offset || (!m_IsSizing && size > m_Buffer.Size - m_Offset) || (size > 0 && !data))
		{
			m_IsValid = false;
			return false;
		}

		if (!m_IsSizing && size > 0)
			std::memcpy(m_Buffer.Data + m_Offset, data, size);

		m_Offset += size;
		return true;
	}

	auto BufferWriter::WriteString(const std::string_view value) -> bool
	{
		if (value.size() > std::numeric_limits<uint32_t>::max())
		{
			m_IsValid = false;
			return false;
		}

		const auto size = static_cast<uint32_t>(value.size());
		return Write(size) && WriteBytes(value.data(), size);
	}
}
