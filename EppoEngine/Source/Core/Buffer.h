#pragma once

#include <cstdint>
#include <cstring>

namespace Eppo
{
	struct Buffer
	{
		uint8_t* Data = nullptr;
		uint32_t Size = 0;

		Buffer() = default;
		Buffer(const Buffer&) = default;

		Buffer(uint32_t size)
		{
			Allocate(size);
		}

		static Buffer Copy(Buffer other)
		{
			Buffer result(other.Size);
			memcpy(result.Data, other.Data, other.Size);
			return result;
		}

		static Buffer Copy(uint8_t* data, uint32_t size)
		{
			Buffer result(size);
			memcpy(result.Data, data, size);
			return result;
		}

		static Buffer Copy(void* data, uint32_t size)
		{
			Buffer result(size);
			memcpy(result.Data, data, size);
			return result;
		}

		void Allocate(uint32_t size)
		{
			Release();

			Data = new uint8_t[size];
			Size = size;
		}

		void Release()
		{
			delete[] Data;
			Data = nullptr;
			Size = 0;
		}

		template<typename T>
		T* As()
		{
			return (T*)Data;
		}

		operator bool() const { return (bool)Data; }
	};

	class ScopedBuffer
	{
	public:
		ScopedBuffer() = default;

		ScopedBuffer(Buffer buffer)
			: m_Buffer(buffer)
		{}

		ScopedBuffer(uint32_t size)
			: m_Buffer(size)
		{}

		~ScopedBuffer()
		{
			m_Buffer.Release();
		}

		uint8_t* Data() { return m_Buffer.Data; }
		uint32_t Size() { return m_Buffer.Size; }

		template<typename T>
		T* As()
		{
			return m_Buffer.As<T>();
		}

	private:
		Buffer m_Buffer;
	};
}
