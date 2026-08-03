#include "Support/EppoTest.h"

#include "Core/Buffer/Buffer.h"

#include <type_traits>

using Eppo::Buffer;
using Eppo::ScopedBuffer;

static_assert(!std::is_copy_constructible_v<ScopedBuffer>);
static_assert(!std::is_copy_assignable_v<ScopedBuffer>);
static_assert(std::is_nothrow_move_constructible_v<ScopedBuffer>);
static_assert(std::is_nothrow_move_assignable_v<ScopedBuffer>);

TEST(Core, Buffer_NullConstruct)
{
    Buffer buffer;
    EXPECT_TRUE(!buffer.Data);
    EXPECT_EQ(0, buffer.Size);
}

TEST(Core, Buffer_ConstructWithSize)
{
    Buffer buffer(256);
    EXPECT_TRUE(buffer.Data);
    EXPECT_EQ(256, buffer.Size);

    buffer.Release();
}

TEST(Core, Buffer_ConstructWithData)
{
    auto data = new uint8_t[256];
    EP_REQUIRE(data);
    for (uint32_t i = 0; i < 256; i++)
        data[i] = static_cast<uint8_t>(rand());

    Buffer buffer(data, 256);
    EP_REQUIRE(buffer.Data);
    EXPECT_EQ(256, buffer.Size);

    for (uint32_t i = 0; i < 256; i++)
        EXPECT_EQ(data[i], buffer.Data[i]);

    buffer.Release();
}

TEST(Core, Buffer_Allocate)
{
    Buffer buffer;
    EXPECT_TRUE(!buffer.Data);
    EXPECT_EQ(0, buffer.Size);

    buffer.Allocate(256);
    EXPECT_TRUE(buffer.Data);
    EXPECT_EQ(256, buffer.Size);

    buffer.Release();
}

TEST(Core, Buffer_Release)
{
    Buffer buffer(256);
    EXPECT_TRUE(buffer.Data);
    EXPECT_EQ(256, buffer.Size);

    buffer.Release();
    EXPECT_TRUE(!buffer.Data);
    EXPECT_EQ(0, buffer.Size);
}

TEST(Core, Buffer_CopyOtherBuffer)
{
    Buffer bufferA(256);
    for (uint32_t i = 0; i < 256; i++)
        bufferA.Data[i] = static_cast<uint8_t>(rand());

    auto bufferB = Buffer::Copy(bufferA);
    EXPECT_TRUE(bufferA.Data);
    EXPECT_TRUE(bufferB.Data);
    EXPECT_EQ(bufferA.Size, bufferB.Size);

    for (uint32_t i = 0; i < 256; i++)
        EXPECT_EQ(bufferA.Data[i], bufferB.Data[i]);

    bufferA.Release();
    bufferB.Release();
}

TEST(Core, Buffer_CopyData)
{
    auto data = new uint8_t[256];
    for (uint32_t i = 0; i < 256; i++)
        data[i] = static_cast<uint8_t>(rand());

    auto buffer = Buffer::Copy(data, 256);
    EXPECT_TRUE(buffer.Data);
    EXPECT_EQ(256, buffer.Size);

    for (uint32_t i = 0; i < 256; i++)
        EXPECT_EQ(data[i], buffer.Data[i]);

    buffer.Release();
}

TEST(Core, Buffer_CastToType)
{
    constexpr uint32_t value = 0x12345678;

    Buffer buffer(256);
    EP_REQUIRE(buffer.Data);
    std::memcpy(buffer.Data, &value, sizeof(value));

    auto* casted = buffer.As<uint32_t>();
    EP_REQUIRE(casted);
    EXPECT_EQ(value, *casted);

    buffer.Release();
}

TEST(Core, ScopedBuffer_MoveTransfersOwnership)
{
    ScopedBuffer source(256);
    auto* data = source.Data();

    ScopedBuffer destination(std::move(source));

    EXPECT_TRUE(!source.Data());
    EXPECT_EQ(0, source.Size());
    EXPECT_TRUE(destination.Data() == data);
    EXPECT_EQ(256, destination.Size());
}

TEST(Core, ScopedBuffer_AdoptBufferClearsSource)
{
    Buffer source(256);
    auto* data = source.Data;

    ScopedBuffer destination(std::move(source));

    EXPECT_TRUE(!source.Data);
    EXPECT_EQ(0, source.Size);
    EXPECT_TRUE(destination.Data() == data);
    EXPECT_EQ(256, destination.Size());
}
