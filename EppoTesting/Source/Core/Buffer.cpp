#include "test.h"

namespace Eppo
{
	static const uint32_t s_DefaultBufferSize = 8;
	//
	// Buffer
	//
	TEST(BufferTest, DefaultConstructor)
	{
		Buffer buffer;

		EXPECT_EQ(buffer.Size, 0);
		EXPECT_EQ(buffer.Data, nullptr);
		EXPECT_FALSE(static_cast<bool>(buffer));
	}

	TEST(BufferTest, ConstructorWithSize)
	{
		Buffer buffer(s_DefaultBufferSize);

		EXPECT_EQ(buffer.Size, s_DefaultBufferSize);
		EXPECT_TRUE(buffer.Data);
		EXPECT_TRUE(static_cast<bool>(buffer));
	}

	TEST(BufferTest, CopyBuffer)
	{
		Buffer originalBuffer(s_DefaultBufferSize);

		Buffer copiedBuffer = Buffer::Copy(originalBuffer);

		EXPECT_EQ(copiedBuffer.Size, s_DefaultBufferSize);
		EXPECT_NE(copiedBuffer.Data, originalBuffer.Data);
		EXPECT_TRUE(copiedBuffer.Data);
		EXPECT_TRUE(static_cast<bool>(copiedBuffer));
	}

	TEST(BufferTest, CopyRawData)
	{
		uint32_t size = 8;
		uint8_t rawData[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

		Buffer buffer = Buffer::Copy(&rawData, size);

		EXPECT_EQ(buffer.Size, size);
		EXPECT_TRUE(buffer.Data);
		EXPECT_TRUE(static_cast<bool>(buffer));

		for (uint32_t i = 0; i < size; i++)
			EXPECT_EQ(buffer.Data[i], rawData[i]);
	}

	TEST(BufferTest, As)
	{
		Buffer buffer(s_DefaultBufferSize);
		for (uint32_t i = 0; i < buffer.Size; i++)
			buffer.Data[i] = i;

		uint8_t* data = buffer.As<uint8_t>();

		for (uint32_t i = 0; i < buffer.Size; i++)
			EXPECT_EQ(data[i], i);
	}

	//
	// ScopedBuffer
	//
	TEST(ScopedBufferTest, DefaultConstructor)
	{
		ScopedBuffer scopedBuffer;

		EXPECT_EQ(scopedBuffer.Size(), 0);
		EXPECT_FALSE(scopedBuffer.Data());
	}

	TEST(ScopedBufferTest, ConstructorWithBuffer)
	{
		Buffer buffer(s_DefaultBufferSize);
		ScopedBuffer scopedBuffer(buffer);

		EXPECT_EQ(scopedBuffer.Size(), s_DefaultBufferSize);
		EXPECT_TRUE(scopedBuffer.Data());
	}

	TEST(ScopedBufferTest, ConstructorWithSize)
	{
		ScopedBuffer scopedBuffer(s_DefaultBufferSize);

		EXPECT_EQ(scopedBuffer.Size(), s_DefaultBufferSize);
		EXPECT_TRUE(scopedBuffer.Data());
	}
}
