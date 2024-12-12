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

		template<typename T>
		void SetData(const T& value, uint32_t offset = 0)
		{
			EPPO_ASSERT(sizeof(T) + offset <= Size);
			memcpy(Data + offset, &value, sizeof(T));
		}

		void SetData(void* data, uint32_t size)
		{
			EPPO_ASSERT(size <= Size);
			memcpy(Data, data, size);
		}

		operator bool() const { return (bool)Data; }
	};

	class ScopedBuffer
	{
	public:
		ScopedBuffer() = default;

		explicit ScopedBuffer(Buffer buffer)
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
		uint32_t Size() const { return m_Buffer.Size; }

		template<typename T>
		T* As()
		{
			return m_Buffer.As<T>();
		}

		template<typename T>
		void SetData(const T& value, uint32_t offset = 0)
		{
			m_Buffer.SetData(value, offset);
		}

		void SetData(void* data, uint32_t size)
		{
			m_Buffer.SetData(data, size);
		}

	private:
		Buffer m_Buffer;
	};
}
