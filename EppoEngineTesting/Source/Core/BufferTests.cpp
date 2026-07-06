#include "Support/EppoTest.h"

#include "Core/Buffer.h"

using namespace Eppo;

// Buffer is a thin owning-by-convention byte span: it allocates on request but
// does not free in a destructor, so these tests Release() explicitly to keep the
// memory tracker honest.
SUITE(Core)
{
    TEST(BufferDefaultIsEmpty)
    {
        const Buffer buffer;
        CHECK(buffer.Data == nullptr);
        CHECK_EQUAL(0u, buffer.Size);
    }

    TEST(BufferSizeCtorAllocates)
    {
        Buffer buffer(16);
        CHECK(buffer.Data != nullptr);
        CHECK_EQUAL(16u, buffer.Size);
        buffer.Release();
    }

    TEST(BufferStoresBytes)
    {
        Buffer buffer(4);
        for (uint8_t i = 0; i < 4; ++i)
            buffer.Data[i] = static_cast<uint8_t>(i + 1);

        CHECK_EQUAL(1u, buffer.Data[0]);
        CHECK_EQUAL(4u, buffer.Data[3]);
        buffer.Release();
    }

    TEST(BufferCopyDuplicatesContentIndependently)
    {
        Buffer source(3);
        source.Data[0] = 10;
        source.Data[1] = 20;
        source.Data[2] = 30;

        Buffer copy = Buffer::Copy(source);
        CHECK(copy.Data != source.Data);
        CHECK_EQUAL(3u, copy.Size);
        CHECK_EQUAL(10u, copy.Data[0]);
        CHECK_EQUAL(30u, copy.Data[2]);

        // Mutating the source must not touch the copy.
        source.Data[0] = 99;
        CHECK_EQUAL(10u, copy.Data[0]);

        source.Release();
        copy.Release();
    }

    TEST(BufferCopyFromRawPointer)
    {
        const uint8_t bytes[] = { 7, 8, 9 };
        Buffer copy = Buffer::Copy(bytes, sizeof(bytes));

        CHECK_EQUAL(3u, copy.Size);
        CHECK_EQUAL(7u, copy.Data[0]);
        CHECK_EQUAL(9u, copy.Data[2]);
        copy.Release();
    }

    TEST(BufferAllocateReplacesStorage)
    {
        Buffer buffer(4);
        buffer.Allocate(8);
        CHECK(buffer.Data != nullptr);
        CHECK_EQUAL(8u, buffer.Size);
        buffer.Release();
    }

    TEST(BufferReleaseResetsToEmpty)
    {
        Buffer buffer(8);
        buffer.Release();
        CHECK(buffer.Data == nullptr);
        CHECK_EQUAL(0u, buffer.Size);
    }
}
