#include "TestSupport/EppoTest.h"
#include "TestSupport/TempDir.h"
#include "StreamTestData.h"

#include "Core/Buffer/FileStreamReader.h"
#include "Core/Buffer/FileStreamWriter.h"

using Eppo::FileStreamReader;
using Eppo::FileStreamWriter;

TEST(Core, FileStreamWriter_RoundTripsThroughFileStreamReader)
{
    const Eppo::Testing::TempDir dir;
    const auto path = dir.File("payload.bin");
    const Eppo::Testing::Payload written = Eppo::Testing::MakePayload();

    uint64_t bytesWritten = 0;
    {
        FileStreamWriter writer(path);
        EP_REQUIRE(writer.IsStreamGood());
        EP_REQUIRE(Eppo::Testing::WritePayload(writer, written));
        bytesWritten = writer.GetStreamPosition();
    }

    FileStreamReader reader(path);
    EP_REQUIRE(reader.IsStreamGood());

    Eppo::Testing::Payload read;
    EP_REQUIRE(Eppo::Testing::ReadPayload(reader, read));

    Eppo::Testing::CheckPayloadEqual(written, read);
    EXPECT_EQ(bytesWritten, reader.GetStreamPosition());
}

TEST(Core, FileStreamWriter_ReportsBadStreamForUnopenablePath)
{
    const Eppo::Testing::TempDir dir;

    FileStreamWriter writer(dir.Path() / "missing" / "payload.bin");
    EXPECT_TRUE(!writer.IsStreamGood());
    EXPECT_TRUE(!writer.WriteData("eppo", 4));
    EXPECT_EQ(0u, writer.GetStreamPosition());
}
