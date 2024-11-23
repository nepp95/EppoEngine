#include "Test.h"

namespace Eppo
{
	//
	// Buffer
	//
	class BufferTestFixture : public testing::TestWithParam<uint32_t>
	{};

	INSTANTIATE_TEST_SUITE_P(BufferTest, BufferTestFixture,
		testing::Values(4, 20, 1024, 2024));

	TEST_P(BufferTestFixture, Constructor)
	{
		Buffer buffer(GetParam());

		EXPECT_EQ(GetParam(), buffer.Size);
		EXPECT_TRUE(buffer.Data);

		buffer.Release();
	}

	TEST(BufferTest, ZeroInitialize)
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
			// Technically a new allocation with a size 0 can have a valid pointer

			buffer.Release();
		}
	}

	TEST_P(BufferTestFixture, Allocate)
	{
		Buffer buffer;
		buffer.Allocate(GetParam());

		EXPECT_EQ(GetParam(), buffer.Size);
		EXPECT_TRUE(buffer.Data);

		buffer.Release();
	}

	TEST_P(BufferTestFixture, Release)
	{
		Buffer buffer(GetParam());

		EXPECT_EQ(GetParam(), buffer.Size);
		EXPECT_TRUE(buffer.Data);

		buffer.Release();

		EXPECT_EQ(0, buffer.Size);
		EXPECT_FALSE(buffer.Data);
	}

	TEST_P(BufferTestFixture, Copy_Buffer)
	{
		Buffer buffer(GetParam());
		memset(buffer.Data, 5, buffer.Size);

		EXPECT_EQ(GetParam(), buffer.Size);
		EXPECT_TRUE(buffer.Data);

		Buffer targetBuffer = Buffer::Copy(buffer);

		EXPECT_EQ(GetParam(), targetBuffer.Size);
		EXPECT_TRUE(targetBuffer.Data);

		for (uint32_t i = 0; i < targetBuffer.Size; i++)
			EXPECT_EQ(5, targetBuffer.Data[i]);

		buffer.Release();
		targetBuffer.Release();
	}

	TEST_P(BufferTestFixture, Copy_DataAndSize)
	{
		uint8_t* data = new uint8_t[GetParam()];
		memset(data, 5, GetParam());

		Buffer targetBuffer = Buffer::Copy(data, GetParam());

		delete[] data;
		data = nullptr;

		EXPECT_EQ(GetParam(), targetBuffer.Size);
		EXPECT_TRUE(targetBuffer.Data);

		for (uint32_t i = 0; i < targetBuffer.Size; i++)
			EXPECT_EQ(5, targetBuffer.Data[i]);

		targetBuffer.Release();
	}

	TEST_P(BufferTestFixture, Copy_VoidDataAndSize)
	{
		void* data = new uint8_t[GetParam()];
		memset(data, 5, GetParam());

		Buffer targetBuffer = Buffer::Copy(data, GetParam());

		EXPECT_EQ(GetParam(), targetBuffer.Size);
		EXPECT_TRUE(targetBuffer.Data);

		for (uint32_t i = 0; i < targetBuffer.Size; i++)
			EXPECT_EQ(5, targetBuffer.Data[i]);

		targetBuffer.Release();
	}

	TEST_P(BufferTestFixture, SetData)
	{
		Buffer buffer(100);
		
		buffer.SetData(GetParam());
		EXPECT_EQ(100, buffer.Size);
		EXPECT_TRUE(buffer.Data);
		EXPECT_EQ(GetParam(), buffer.As<uint16_t>()[0]);

		buffer.SetData(GetParam(), 50);
		EXPECT_EQ(100, buffer.Size);
		EXPECT_TRUE(buffer.Data);
		EXPECT_EQ(GetParam(), buffer.As<uint16_t>()[25]);

		buffer.Release();
	}

	//
	// ScopedBuffer
	//
	class ScopedBufferTestFixture : public testing::TestWithParam<uint32_t>
	{};

	INSTANTIATE_TEST_SUITE_P(ScopedBufferTest, ScopedBufferTestFixture,
		testing::Values(4, 20, 1024, 2024));

	TEST(ScopedBufferTest, Constructor)
	{
		ScopedBuffer buffer;

		EXPECT_EQ(0, buffer.Size());
		EXPECT_FALSE(buffer.Data());
	}

	TEST_P(ScopedBufferTestFixture, Constructor_Int)
	{
		ScopedBuffer buffer(GetParam());

		EXPECT_EQ(GetParam(), buffer.Size());
		EXPECT_TRUE(buffer.Data());
	}

	TEST_P(ScopedBufferTestFixture, Constructor_Buffer)
	{
		Buffer buffer(GetParam());
		
		EXPECT_EQ(GetParam(), buffer.Size);
		EXPECT_TRUE(buffer.Data);

		ScopedBuffer scopedBuffer(buffer);

		EXPECT_EQ(GetParam(), scopedBuffer.Size());
		EXPECT_TRUE(scopedBuffer.Data());
	}

	TEST_P(ScopedBufferTestFixture, SetData)
	{
		ScopedBuffer buffer(100);

		buffer.SetData(GetParam());
		EXPECT_EQ(100, buffer.Size());
		EXPECT_TRUE(buffer.Data());
		EXPECT_EQ(GetParam(), buffer.As<uint16_t>()[0]);

		buffer.SetData(GetParam(), 50);
		EXPECT_EQ(100, buffer.Size());
		EXPECT_TRUE(buffer.Data());
		EXPECT_EQ(GetParam(), buffer.As<uint16_t>()[25]);
	}
}
