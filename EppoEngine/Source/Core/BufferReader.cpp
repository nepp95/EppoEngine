#include "pch.h"
#include "Core/BufferReader.h"

namespace Eppo
{
	BufferReader::BufferReader(const Buffer& buffer)
		: m_Buffer(buffer)
	{}

	BufferReader::BufferReader(const Buffer& buffer, const bool isValid)
		: m_Buffer(buffer), m_IsValid(isValid)
	{}

	auto BufferReader::ReadBytes(void* data, const uint64_t size) -> bool
	{
		if (!CanRead(size) || (size > 0 && !data))
		{
			m_IsValid = false;
			return false;
		}

		if (size > 0)
			std::memcpy(data, m_Buffer.Data + m_Offset, size);

		m_Offset += size;
		return true;
	}

	auto BufferReader::ReadString() -> std::string
	{
		uint32_t size = 0;
		if (!Read(size) || !CanRead(size))
			return {};

		std::string value(size, '\0');
		if (!ReadBytes(value.data(), size))
			return {};

		return value;
	}

	auto BufferReader::ReadSubReader(const uint64_t size) -> BufferReader
	{
		if (!CanRead(size))
			return BufferReader(Buffer(nullptr, 0), false);

		uint8_t* data = m_Buffer.Data ? m_Buffer.Data + m_Offset : nullptr;
		m_Offset += size;
		return BufferReader(Buffer(data, size), true);
	}

	auto BufferReader::CanRead(const uint64_t size) -> bool
	{
		if (!m_IsValid || size > m_Buffer.Size - m_Offset)
		{
			m_IsValid = false;
			return false;
		}

		return true;
	}
}
