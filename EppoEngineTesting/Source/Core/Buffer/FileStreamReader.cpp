#include "TestSupport/EppoTest.h"
#include "TestSupport/TempDir.h"

#include "Core/Buffer/FileStreamReader.h"
#include "Core/Buffer/FileStreamWriter.h"

using Eppo::FileStreamReader;
using Eppo::FileStreamWriter;

TEST(Core, FileStreamReader_ReportsBadStreamForMissingFile)
{
    const Eppo::Testing::TempDir dir;

    const FileStreamReader reader(dir.File("missing.bin"));
    EXPECT_TRUE(!reader.IsStreamGood());
}

TEST(Core, FileStreamReader_ReadDataFailsPastEndOfFile)
{
    const Eppo::Testing::TempDir dir;
    const auto path = dir.File("short.bin");

    {
        FileStreamWriter writer(path);
        EP_REQUIRE(writer.WriteData("eppo", 4));
    }

    FileStreamReader reader(path);
    EP_REQUIRE(reader.IsStreamGood());

    std::array<char, 8> data{};
    EXPECT_TRUE(!reader.ReadData(data.data(), data.size()));
}
