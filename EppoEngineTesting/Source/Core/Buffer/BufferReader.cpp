#include "TestSupport/EppoTest.h"

#include "Core/Buffer/BufferReader.h"

using Eppo::Buffer;
using Eppo::BufferReader;

// The Stream round-trips exercise the happy path transitively; these pin the reader edges the
// round-trips never hit: the size==0 short-circuit, reading from a non-zero start offset, and
// the exact-capacity boundary where the guard is still true.

TEST(Core, BufferReader_ZeroSizeRead_SucceedsWithoutTouchingOutput)
{
    Buffer buffer(4);
    std::memset(buffer.Data, 0xAB, buffer.Size);
    BufferReader reader(buffer);

    char scratch = 0x7F;
    EXPECT_TRUE(reader.ReadData(&scratch, 0));
    EXPECT_EQ(0u, reader.GetStreamPosition());
    EXPECT_EQ(0x7F, static_cast<uint8_t>(scratch));

    buffer.Release();
}

TEST(Core, BufferReader_NonZeroOffset_ReadsSubrangeFromOffset)
{
    const std::array<uint8_t, 4> source{ 1, 2, 3, 4 };
    Buffer buffer(source.size());
    std::memcpy(buffer.Data, source.data(), source.size());
    BufferReader reader(buffer, 1);

    std::array<uint8_t, 3> destination{};
    EXPECT_TRUE(reader.ReadData(reinterpret_cast<char*>(destination.data()), destination.size()));
    EXPECT_EQ(4u, reader.GetStreamPosition());
    const std::array<uint8_t, 3> expected{ 2, 3, 4 };
    EP_EXPECT_ARRAY_EQ(expected.data(), destination.data(), expected.size());

    buffer.Release();
}

TEST(Core, BufferReader_BoundaryRead_ConsumesExactlyToEnd)
{
    const std::array<uint8_t, 4> source{ 1, 2, 3, 4 };
    Buffer buffer(source.size());
    std::memcpy(buffer.Data, source.data(), source.size());
    BufferReader reader(buffer);

    std::array<uint8_t, 4> destination{};
    EXPECT_TRUE(reader.ReadData(reinterpret_cast<char*>(destination.data()), destination.size()));
    EXPECT_EQ(source.size(), reader.GetStreamPosition());
    EP_EXPECT_ARRAY_EQ(source.data(), destination.data(), source.size());

    buffer.Release();
}
