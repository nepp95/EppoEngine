#pragma once

#include "TestSupport/EppoTest.h"

#include "Core/Buffer/StreamReader.h"
#include "Core/Buffer/StreamWriter.h"

// Shared fixture data for the stream round-trip tests, which inherently span a writer, a reader,
// and (for the cross-backing test) both backings. Kept here so BufferWriter/FileStreamWriter/
// StreamWriter test files can each drive the same payload.
namespace Eppo::Testing
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

    static_assert(StreamSerializable<Marker>);
    static_assert(!StreamSerializable<Unserializable>);
    static_assert(StreamDeserializable<Marker>);
    static_assert(!StreamDeserializable<Unserializable>);

    struct Payload
    {
        uint32_t Magic = 0;
        std::string Text;
        std::map<uint32_t, uint64_t> Counts;
        Marker Mark;
    };

    inline auto MakePayload() -> Payload
    {
        return Payload{
            .Magic = 0x12345678,
            .Text = "payload",
            .Counts = { { 1, 100 }, { 2, 200 } },
            .Mark = Marker{ 7,          "marker"   },
        };
    }

    inline auto WritePayload(StreamWriter& writer, const Payload& payload) -> bool
    {
        return writer.WriteRaw(payload.Magic) && writer.WriteString(payload.Text) && writer.WriteZero(4) &&
            writer.WriteMap(payload.Counts) && writer.WriteObject(payload.Mark);
    }

    inline auto ReadPayload(StreamReader& reader, Payload& payload) -> bool
    {
        std::array<char, 4> padding{};

        return reader.ReadRaw(payload.Magic) && reader.ReadString(payload.Text) && reader.ReadData(padding.data(), padding.size()) &&
            reader.ReadMap(payload.Counts) && reader.ReadObject(payload.Mark);
    }

    inline auto CheckPayloadEqual(const Payload& expected, const Payload& actual) -> void
    {
        EXPECT_EQ(expected.Magic, actual.Magic);
        EXPECT_EQ(expected.Text, actual.Text);
        EP_REQUIRE_EQ(expected.Counts.size(), actual.Counts.size());
        for (const auto& [key, value] : expected.Counts)
        {
            EP_REQUIRE(actual.Counts.contains(key));
            EXPECT_EQ(value, actual.Counts.at(key));
        }
        EXPECT_EQ(expected.Mark.Id, actual.Mark.Id);
        EXPECT_EQ(expected.Mark.Name, actual.Mark.Name);
    }
}
