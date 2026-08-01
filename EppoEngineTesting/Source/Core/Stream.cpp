#include "Support/EppoTest.h"
#include "Support/TempDir.h"

#include "Core/Buffer/BufferReader.h"
#include "Core/Buffer/BufferWriter.h"
#include "Core/Buffer/FileStreamReader.h"
#include "Core/Buffer/FileStreamWriter.h"
#include "Core/Buffer/StreamReader.h"
#include "Core/Buffer/StreamWriter.h"

SUITE(Core)
{
    using Eppo::Buffer;
    using Eppo::BufferReader;
    using Eppo::BufferWriter;
    using Eppo::FileStreamReader;
    using Eppo::FileStreamWriter;
    using Eppo::StreamReader;
    using Eppo::StreamWriter;

    namespace
    {
        struct Marker
        {
            uint32_t Id = 0;
            std::string Name;

            static auto Serialize(StreamWriter* writer, const Marker& value) -> bool
            {
                return writer->WriteRaw(value.Id) && writer->WriteString(value.Name);
            }

            static auto Deserialize(StreamReader* reader, Marker& value) -> bool
            {
                return reader->ReadRaw(value.Id) && reader->ReadString(value.Name);
            }
        };

        struct Unserializable
        {};

        static_assert(Eppo::StreamSerializable<Marker>);
        static_assert(!Eppo::StreamSerializable<Unserializable>);
        static_assert(Eppo::StreamDeserializable<Marker>);
        static_assert(!Eppo::StreamDeserializable<Unserializable>);

        struct Payload
        {
            uint32_t Magic = 0;
            std::string Text;
            std::map<uint32_t, uint64_t> Counts;
            Marker Mark;
        };

        auto MakePayload() -> Payload
        {
            return Payload{
                .Magic = 0x12345678,
                .Text = "payload",
                .Counts = { { 1, 100 }, { 2, 200 } },
                .Mark = Marker{ 7,          "marker"   },
            };
        }

        auto WritePayload(StreamWriter& writer, const Payload& payload) -> bool
        {
            return writer.WriteRaw(payload.Magic) && writer.WriteString(payload.Text) && writer.WriteZero(4) &&
                writer.WriteMap(payload.Counts) && writer.WriteObject(payload.Mark);
        }

        auto ReadPayload(StreamReader& reader, Payload& payload) -> bool
        {
            std::array<char, 4> padding{};

            return reader.ReadRaw(payload.Magic) && reader.ReadString(payload.Text) && reader.ReadData(padding.data(), padding.size()) &&
                reader.ReadMap(payload.Counts) && reader.ReadObject(payload.Mark);
        }

        auto CheckPayloadEqual(const Payload& expected, const Payload& actual) -> void
        {
            CHECK_EQUAL(expected.Magic, actual.Magic);
            CHECK_EQUAL(expected.Text, actual.Text);
            REQUIRE CHECK_EQUAL(expected.Counts.size(), actual.Counts.size());
            for (const auto& [key, value] : expected.Counts)
            {
                REQUIRE CHECK(actual.Counts.contains(key));
                CHECK_EQUAL(value, actual.Counts.at(key));
            }
            CHECK_EQUAL(expected.Mark.Id, actual.Mark.Id);
            CHECK_EQUAL(expected.Mark.Name, actual.Mark.Name);
        }
    }

    TEST(BufferWriter_RoundTripsThroughBufferReader)
    {
        const Payload written = MakePayload();

        BufferWriter writer(256);
        REQUIRE CHECK(writer.IsStreamGood());
        REQUIRE CHECK(WritePayload(writer, written));

        BufferReader reader(writer.GetBuffer());
        Payload read;
        REQUIRE CHECK(ReadPayload(reader, read));

        CheckPayloadEqual(written, read);
        CHECK_EQUAL(writer.GetStreamPosition(), reader.GetStreamPosition());
    }

    TEST(FileStreamWriter_RoundTripsThroughFileStreamReader)
    {
        const Eppo::Testing::TempDir dir;
        const auto path = dir.File("payload.bin");
        const Payload written = MakePayload();

        uint64_t bytesWritten = 0;
        {
            FileStreamWriter writer(path);
            REQUIRE CHECK(writer.IsStreamGood());
            REQUIRE CHECK(WritePayload(writer, written));
            bytesWritten = writer.GetStreamPosition();
        }

        FileStreamReader reader(path);
        REQUIRE CHECK(reader.IsStreamGood());

        Payload read;
        REQUIRE CHECK(ReadPayload(reader, read));

        CheckPayloadEqual(written, read);
        CHECK_EQUAL(bytesWritten, reader.GetStreamPosition());
    }

    TEST(StreamWriter_SerializesIdenticallyThroughBothBackings)
    {
        const Eppo::Testing::TempDir dir;
        const auto path = dir.File("payload.bin");
        const Payload payload = MakePayload();

        BufferWriter bufferWriter(256);
        REQUIRE CHECK(WritePayload(bufferWriter, payload));

        {
            FileStreamWriter fileWriter(path);
            REQUIRE CHECK(WritePayload(fileWriter, payload));
            CHECK_EQUAL(bufferWriter.GetStreamPosition(), fileWriter.GetStreamPosition());
        }

        const Buffer bufferBytes = bufferWriter.GetBuffer();
        const auto fileBytes = Eppo::FS::ReadBytes(path);
        REQUIRE CHECK_EQUAL(bufferBytes.Size, fileBytes.size());
        CHECK_ARRAY_EQUAL(bufferBytes.As<char>(), fileBytes.data(), bufferBytes.Size);
    }

    TEST(FileStreamWriter_ReportsBadStreamForUnopenablePath)
    {
        const Eppo::Testing::TempDir dir;

        FileStreamWriter writer(dir.Path() / "missing" / "payload.bin");
        CHECK(!writer.IsStreamGood());
        CHECK(!writer.WriteData("eppo", 4));
        CHECK_EQUAL(0, writer.GetStreamPosition());
    }

    // Every other test round-trips through the same build, so a change to the length prefix would move
    // reader and writer together and stay invisible. This pins the bytes themselves.
    TEST(StreamWriter_WritesStringsWithAUint64LengthPrefix)
    {
        BufferWriter writer(64);
        REQUIRE CHECK(writer.WriteString("ab"));

        constexpr std::array<uint8_t, 10> expected{ 0x02, 0, 0, 0, 0, 0, 0, 0, 'a', 'b' };
        const Buffer written = writer.GetBuffer();
        REQUIRE CHECK_EQUAL(expected.size(), written.Size);
        CHECK_ARRAY_EQUAL(expected.data(), written.Data, expected.size());
    }

    TEST(FileStreamReader_ReportsBadStreamForMissingFile)
    {
        const Eppo::Testing::TempDir dir;

        const FileStreamReader reader(dir.File("missing.bin"));
        CHECK(!reader.IsStreamGood());
    }

    TEST(FileStreamReader_ReadDataFailsPastEndOfFile)
    {
        const Eppo::Testing::TempDir dir;
        const auto path = dir.File("short.bin");

        {
            FileStreamWriter writer(path);
            REQUIRE CHECK(writer.WriteData("eppo", 4));
        }

        FileStreamReader reader(path);
        REQUIRE CHECK(reader.IsStreamGood());

        std::array<char, 8> data{};
        CHECK(!reader.ReadData(data.data(), data.size()));
    }
}
