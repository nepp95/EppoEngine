#include "TestSupport/EppoTest.h"

#include "Core/Buffer/Buffer.h"

using Eppo::Buffer;

namespace
{
    constexpr uint64_t BufferSize = 1024;
}

TEST(Core, Buffer_NullConstruct)
{
    // Arrange
    // Act
    Buffer buffer;

    // Assert
    EXPECT_FALSE(buffer.Data);
    EXPECT_EQ(0, buffer.Size);
}

TEST(Core, Buffer_ConstructWithSize)
{
    // Arrange
    // Act
    Buffer buffer(BufferSize);

    // Assert
    EXPECT_TRUE(buffer.Data);
    EXPECT_EQ(BufferSize, buffer.Size);

    buffer.Release();
}

TEST(Core, Buffer_ConstructWithBufferAndSize)
{
    // Arrange
    Buffer srcBuffer(BufferSize / 2);

    // Act
    Buffer dstBuffer(srcBuffer, srcBuffer.Size);

    // Assert
    EXPECT_TRUE(dstBuffer.Data);
    EXPECT_EQ(srcBuffer.Data, dstBuffer.Data);
    EXPECT_EQ(srcBuffer.Size, dstBuffer.Size);

    srcBuffer.Release();
}

TEST(Core, Buffer_ConstructWithBufferAndBiggerSize)
{
    // Arrange
    Buffer srcBuffer(BufferSize / 2);

    // Act
    Buffer dstBuffer(srcBuffer, BufferSize);

    // Assert
    EXPECT_TRUE(dstBuffer.Data);
    EXPECT_EQ(srcBuffer.Data, dstBuffer.Data);
    EXPECT_EQ(BufferSize / 2, dstBuffer.Size);

    srcBuffer.Release();
}

TEST(Core, Buffer_ConstructWithPointerAndSize)
{
    // Arrange
    uint8_t* data = new uint8_t[BufferSize];

    // Act
    Buffer buffer(data, BufferSize);

    // Assert
    EXPECT_TRUE(buffer.Data);
    EXPECT_EQ(data, buffer.Data);
    EXPECT_EQ(BufferSize, buffer.Size);

    buffer.Release();
}

TEST(Core, Buffer_Allocate)
{
    // Arrange
    Buffer buffer;

    // Act
    buffer.Allocate(BufferSize);

    // Assert
    EXPECT_TRUE(buffer.Data);
    EXPECT_EQ(BufferSize, buffer.Size);

    buffer.Release();
}

TEST(Core, Buffer_AllocateTwice)
{
    // Arrange
    Buffer buffer;
    buffer.Allocate(BufferSize);
    auto oldPtr = buffer.Data;

    // Act
    buffer.Allocate(BufferSize / 2);

    // Assert
    EXPECT_TRUE(buffer.Data);
    EXPECT_NE(oldPtr, buffer.Data);
    EXPECT_EQ(BufferSize / 2, buffer.Size);

    buffer.Release();
}

TEST(Core, Buffer_Release)
{
    // Arrange
    Buffer buffer(BufferSize);

    // Act
    buffer.Release();

    // Assert
    EXPECT_FALSE(buffer.Data);
    EXPECT_EQ(0, buffer.Size);
}

TEST(Core, Buffer_CopyOtherBuffer)
{
    // Arrange
    Buffer srcBuffer(BufferSize);
    for (auto i = 0; i < BufferSize; i++)
        srcBuffer.Data[i] = static_cast<uint8_t>(rand());

    // Act
    Buffer dstBuffer = Buffer::Copy(srcBuffer);

    // Assert
    EXPECT_TRUE(dstBuffer.Data);
    EXPECT_EQ(srcBuffer.Size, dstBuffer.Size);

    for (auto i = 0; i < BufferSize; i++)
        EXPECT_EQ(srcBuffer.Data[i], dstBuffer.Data[i]);

    srcBuffer.Release();
    dstBuffer.Release();
}

TEST(Core, Buffer_CopyData)
{
    // Arrange
    uint8_t* data = new uint8_t[BufferSize];
    for (auto i = 0; i < BufferSize; i++)
        data[i] = static_cast<uint8_t>(rand());

    // Act
    Buffer buffer = Buffer::Copy(data, BufferSize);

    // Assert
    EXPECT_TRUE(buffer.Data);
    EXPECT_EQ(BufferSize, buffer.Size);

    for (auto i = 0; i < BufferSize; i++)
        EXPECT_EQ(data[i], buffer.Data[i]);

    delete[] data;
    buffer.Release();
}
