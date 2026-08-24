#include "TestSupport/EppoTest.h"
#include "TestSupport/TempDir.h"
#include "StreamTestData.h"

#include "Core/Buffer/BufferWriter.h"
#include "Core/Buffer/FileStreamWriter.h"

using Eppo::Buffer;
using Eppo::BufferWriter;
using Eppo::FileStreamWriter;

TEST(Core, StreamWriter_SerializesIdenticallyThroughBothBackings)
{
    const Eppo::Testing::TempDir dir;
    const auto path = dir.File("payload.bin");
    const Eppo::Testing::Payload payload = Eppo::Testing::MakePayload();

    BufferWriter bufferWriter(256);
    EP_REQUIRE(Eppo::Testing::WritePayload(bufferWriter, payload));

    {
        FileStreamWriter fileWriter(path);
        EP_REQUIRE(Eppo::Testing::WritePayload(fileWriter, payload));
        EXPECT_EQ(bufferWriter.GetStreamPosition(), fileWriter.GetStreamPosition());
    }

    const Buffer bufferBytes = bufferWriter.GetBuffer();
    const auto fileBytes = Eppo::FS::ReadBytes(path);
    EP_REQUIRE_EQ(bufferBytes.Size, fileBytes.size());
    EP_EXPECT_ARRAY_EQ(bufferBytes.As<char>(), fileBytes.data(), bufferBytes.Size);
}

// Every other test round-trips through the same build, so a change to the length prefix would move
// reader and writer together and stay invisible. This pins the bytes themselves.
TEST(Core, StreamWriter_WritesStringsWithAUint64LengthPrefix)
{
    BufferWriter writer(64);
    EP_REQUIRE(writer.WriteString("ab"));

    constexpr std::array<uint8_t, 10> expected{ 0x02, 0, 0, 0, 0, 0, 0, 0, 'a', 'b' };
    const Buffer written = writer.GetBuffer();
    EP_REQUIRE_EQ(expected.size(), written.Size);
    EP_EXPECT_ARRAY_EQ(expected.data(), written.Data, expected.size());
}
