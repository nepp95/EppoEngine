#include "Test.h"

namespace Eppo
{
	//
	// Buffer
	//
	TEST(BufferTest, Constructor)
	{
		// Default
		{
			Buffer buffer;

			EXPECT_EQ(0, buffer.Size);
			EXPECT_FALSE(buffer.Data);
		
			buffer.Release();
		}

		// Zero initialize
		{
			Buffer buffer(0);
			
			EXPECT_EQ(0, buffer.Size);

			buffer.Release();
		}

		// Parameterized
		{
			Buffer buffer(1024);

			EXPECT_EQ(1024, buffer.Size);
			EXPECT_TRUE(buffer.Data);

			buffer.Release();
		}
	}

	TEST(BufferTest, Allocate)
	{
		Buffer buffer;
		buffer.Allocate(1024);

		EXPECT_EQ(1024, buffer.Size);
		EXPECT_TRUE(buffer.Data);

		buffer.Release();
	}

	TEST(BufferTest, Release)
	{
		Buffer buffer(1024);

		EXPECT_EQ(1024, buffer.Size);
		EXPECT_TRUE(buffer.Data);

		buffer.Release();

		EXPECT_EQ(0, buffer.Size);
		EXPECT_FALSE(buffer.Data);
	}

	TEST(BufferTest, Copy_Buffer)
	{
		Buffer buffer(1024);
		memset(buffer.Data, 5, buffer.Size);

		EXPECT_EQ(1024, buffer.Size);
		EXPECT_TRUE(buffer.Data);

		Buffer targetBuffer = Buffer::Copy(buffer);

		EXPECT_EQ(1024, targetBuffer.Size);
		EXPECT_TRUE(targetBuffer.Data);

		for (uint32_t i = 0; i < targetBuffer.Size; i++)
			EXPECT_EQ(5, targetBuffer.Data[i]);

		buffer.Release();
		targetBuffer.Release();
	}

	TEST(BufferTest, Copy_DataAndSize)
	{
		uint8_t* data = new uint8_t[1024];
		memset(data, 5, 1024);

		Buffer targetBuffer = Buffer::Copy(data, 1024);

		delete[] data;
		data = nullptr;

		EXPECT_EQ(1024, targetBuffer.Size);
		EXPECT_TRUE(targetBuffer.Data);

		for (uint32_t i = 0; i < targetBuffer.Size; i++)
			EXPECT_EQ(5, targetBuffer.Data[i]);

		targetBuffer.Release();
	}

	TEST(BufferTest, Copy_VoidDataAndSize)
	{
		void* data = new uint8_t[1024];
		memset(data, 5, 1024);

		Buffer targetBuffer = Buffer::Copy(data, 1024);

		EXPECT_EQ(1024, targetBuffer.Size);
		EXPECT_TRUE(targetBuffer.Data);

		for (uint32_t i = 0; i < targetBuffer.Size; i++)
			EXPECT_EQ(5, targetBuffer.Data[i]);

		targetBuffer.Release();
	}

	//
	// ScopedBuffer
	//
	TEST(ScopedBufferTest, Constructor)
	{
		ScopedBuffer buffer;

		EXPECT_EQ(0, buffer.Size());
		EXPECT_FALSE(buffer.Data());
	}

	TEST(ScopedBufferTest, Constructor_Int)
	{
		{
			ScopedBuffer buffer(0);

			EXPECT_EQ(0, buffer.Size());
		}

		{
			ScopedBuffer buffer(1024);

			EXPECT_EQ(1024, buffer.Size());
			EXPECT_TRUE(buffer.Data());
		}
	}

	TEST(ScopedBufferTest, Constructor_Buffer)
	{
		Buffer buffer(1024);
		
		EXPECT_EQ(1024, buffer.Size);
		EXPECT_TRUE(buffer.Data);

		ScopedBuffer scopedBuffer(buffer);
		buffer.Release();

		EXPECT_EQ(1024, scopedBuffer.Size());
		EXPECT_TRUE(scopedBuffer.Data());
	}
}
