#include "TestSupport/EppoTest.h"
#include "StreamTestData.h"

#include "Core/Buffer/BufferReader.h"
#include "Core/Buffer/BufferWriter.h"

using Eppo::Buffer;
using Eppo::BufferReader;
using Eppo::BufferWriter;

// Writer edges the round-trip never hits: the size==0 short-circuit, offset accumulation across
// sequential writes, the exact-capacity boundary, and non-owning destruction leaving the caller's
// buffer alone.

TEST(Core, BufferWriter_ZeroSizeWrite_SucceedsWithoutAdvancing)
{
    BufferWriter writer(4);
    EXPECT_TRUE(writer.WriteData("x", 0));
    EXPECT_EQ(0u, writer.GetStreamPosition());
}

TEST(Core, BufferWriter_SequentialWrites_AccumulateOffsetAndPartialFill)
{
    BufferWriter writer(8);
    EP_REQUIRE(writer.WriteData("ab", 2));
    EP_REQUIRE(writer.WriteData("cde", 3));

    // The second write starts at the first's end (m_Offset += size), and GetBuffer() reports the
    // partial fill, not the full capacity.
    EXPECT_EQ(5u, writer.GetStreamPosition());
    const Buffer written = writer.GetBuffer();
    EXPECT_EQ(5u, written.Size);
    const std::array<uint8_t, 5> expected{ 'a', 'b', 'c', 'd', 'e' };
    EP_EXPECT_ARRAY_EQ(expected.data(), written.Data, expected.size());
}

TEST(Core, BufferWriter_BoundaryWrite_FillsExactCapacity)
{
    const std::array<uint8_t, 4> source{ 9, 8, 7, 6 };
    BufferWriter writer(source.size());
    EXPECT_TRUE(writer.WriteData(reinterpret_cast<const char*>(source.data()), source.size()));
    EXPECT_EQ(source.size(), writer.GetStreamPosition());

    const Buffer written = writer.GetBuffer();
    EP_EXPECT_ARRAY_EQ(source.data(), written.Data, source.size());
}

TEST(Core, BufferWriter_NonOwningBuffer_LeavesCallerBufferValidAfterDestruction)
{
    Buffer buffer(8);
    std::memset(buffer.Data, 0, buffer.Size);
    {
        BufferWriter writer(buffer);
        EP_REQUIRE(writer.WriteData("test", 4));
    }

    // The non-owning ctor must not release the caller's buffer on destruction: the bytes it
    // wrote are still readable here (a release would make this a use-after-free). This pin has
    // real teeth only under a heap checker (e.g. ASan); without one a stray double-free may not
    // fault deterministically.
    EXPECT_EQ('t', buffer.As<char>()[0]);
    EXPECT_EQ('e', buffer.As<char>()[1]);
    EXPECT_EQ('s', buffer.As<char>()[2]);
    EXPECT_EQ('t', buffer.As<char>()[3]);

    buffer.Release();
}

TEST(Core, BufferWriter_RoundTripsThroughBufferReader)
{
    const Eppo::Testing::Payload written = Eppo::Testing::MakePayload();

    BufferWriter writer(256);
    EP_REQUIRE(writer.IsStreamGood());
    EP_REQUIRE(Eppo::Testing::WritePayload(writer, written));

    BufferReader reader(writer.GetBuffer());
    Eppo::Testing::Payload read;
    EP_REQUIRE(Eppo::Testing::ReadPayload(reader, read));

    Eppo::Testing::CheckPayloadEqual(written, read);
    EXPECT_EQ(writer.GetStreamPosition(), reader.GetStreamPosition());
}
