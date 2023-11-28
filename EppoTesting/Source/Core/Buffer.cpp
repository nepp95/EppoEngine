#include "test.h"

namespace Eppo
{
	//
	// Buffer
	//
	TEST(BufferTest, ZeroInitialized)
	{
		Buffer buffer;

		EXPECT_EQ(0, buffer.Size);
		EXPECT_FALSE(buffer.Data);
	}

	TEST(BufferTest, ZeroInitialized_0)
	{
		Buffer buffer(0);

		EXPECT_EQ(0, buffer.Size);
		EXPECT_FALSE(buffer.Data);
	}

	TEST(BufferTest, Initialize_1024)
	{
		Buffer buffer(1024);

		EXPECT_EQ(1024, buffer.Size);
		EXPECT_TRUE(buffer.Data);
	}

	TEST(BufferTest, Allocate_1024)
	{
		Buffer buffer;
		buffer.Allocate(1024);

		EXPECT_EQ(1024, buffer.Size);
		EXPECT_TRUE(buffer.Data);
	}

	TEST(BufferTest, Release)
	{
		Buffer buffer;
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
	}

	//
	// ScopedBuffer
	//
	TEST(ScopedBufferTest, ZeroInitialized)
	{
		ScopedBuffer buffer;

		EXPECT_EQ(0, buffer.Size());
		EXPECT_FALSE(buffer.Data());
	}

	TEST(ScopedBufferTest, ZeroInitialized_0)
	{
		ScopedBuffer buffer(0);

		EXPECT_EQ(0, buffer.Size());
		EXPECT_FALSE(buffer.Data());
	}

	TEST(ScopedBufferTest, Initialize_1024)
	{
		ScopedBuffer buffer(1024);

		EXPECT_EQ(1024, buffer.Size());
		EXPECT_TRUE(buffer.Data());
	}
}