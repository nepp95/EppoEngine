#include "Support/EppoTest.h"

#include "Core/Buffer/Buffer.h"

#include <type_traits>

SUITE(Core)
{
    using Eppo::Buffer;
    using Eppo::ScopedBuffer;

    static_assert(!std::is_copy_constructible_v<ScopedBuffer>);
    static_assert(!std::is_copy_assignable_v<ScopedBuffer>);
    static_assert(std::is_nothrow_move_constructible_v<ScopedBuffer>);
    static_assert(std::is_nothrow_move_assignable_v<ScopedBuffer>);

    TEST(Buffer_NullConstruct)
    {
        Buffer buffer;
        CHECK(!buffer.Data);
        CHECK_EQUAL(0, buffer.Size);
    }

    TEST(Buffer_ConstructWithSize)
    {
        Buffer buffer(256);
        CHECK(buffer.Data);
        CHECK_EQUAL(256, buffer.Size);

        buffer.Release();
    }

    TEST(Buffer_ConstructWithData)
    {
        auto data = new uint8_t[256];
        REQUIRE CHECK(data);
        for (uint32_t i = 0; i < 256; i++)
            data[i] = static_cast<uint8_t>(rand());

        Buffer buffer(data, 256);
        REQUIRE CHECK(buffer.Data);
        CHECK_EQUAL(256, buffer.Size);

        for (uint32_t i = 0; i < 256; i++)
            CHECK_EQUAL(data[i], buffer.Data[i]);

        buffer.Release();
    }

    TEST(Buffer_Allocate)
    {
        Buffer buffer;
        CHECK(!buffer.Data);
        CHECK_EQUAL(0, buffer.Size);

        buffer.Allocate(256);
        CHECK(buffer.Data);
        CHECK_EQUAL(256, buffer.Size);

        buffer.Release();
    }

    TEST(Buffer_Release)
    {
        Buffer buffer(256);
        CHECK(buffer.Data);
        CHECK_EQUAL(256, buffer.Size);

        buffer.Release();
        CHECK(!buffer.Data);
        CHECK_EQUAL(0, buffer.Size);
    }

    TEST(Buffer_CopyOtherBuffer)
    {
        Buffer bufferA(256);
        for (uint32_t i = 0; i < 256; i++)
            bufferA.Data[i] = static_cast<uint8_t>(rand());

        auto bufferB = Buffer::Copy(bufferA);
        CHECK(bufferA.Data);
        CHECK(bufferB.Data);
        CHECK_EQUAL(bufferA.Size, bufferB.Size);

        for (uint32_t i = 0; i < 256; i++)
            CHECK_EQUAL(bufferA.Data[i], bufferB.Data[i]);

        bufferA.Release();
        bufferB.Release();
    }

    TEST(Buffer_CopyData)
    {
        auto data = new uint8_t[256];
        for (uint32_t i = 0; i < 256; i++)
            data[i] = static_cast<uint8_t>(rand());

        auto buffer = Buffer::Copy(data, 256);
        CHECK(buffer.Data);
        CHECK_EQUAL(256, buffer.Size);

        for (uint32_t i = 0; i < 256; i++)
            CHECK_EQUAL(data[i], buffer.Data[i]);

        buffer.Release();
    }

    TEST(Buffer_CastToType)
    {
        constexpr uint32_t value = 0x12345678;

        Buffer buffer(256);
        REQUIRE CHECK(buffer.Data);
        std::memcpy(buffer.Data, &value, sizeof(value));

        auto* casted = buffer.As<uint32_t>();
        REQUIRE CHECK(casted);
        CHECK_EQUAL(value, *casted);

        buffer.Release();
    }

    TEST(ScopedBuffer_MoveTransfersOwnership)
    {
        ScopedBuffer source(256);
        auto* data = source.Data();

        ScopedBuffer destination(std::move(source));

        CHECK(!source.Data());
        CHECK_EQUAL(0, source.Size());
        CHECK(destination.Data() == data);
        CHECK_EQUAL(256, destination.Size());
    }

    TEST(ScopedBuffer_AdoptBufferClearsSource)
    {
        Buffer source(256);
        auto* data = source.Data;

        ScopedBuffer destination(std::move(source));

        CHECK(!source.Data);
        CHECK_EQUAL(0, source.Size);
        CHECK(destination.Data() == data);
        CHECK_EQUAL(256, destination.Size());
    }
}
