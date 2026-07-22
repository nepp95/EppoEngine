#include "Support/EppoTest.h"

#include "Core/BufferReader.h"
#include "Core/BufferWriter.h"

SUITE(Core)
{
    using Eppo::Buffer;
    using Eppo::BufferReader;
    using Eppo::BufferWriter;

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

    TEST(BufferWriter_SizingMode_ReturnsExactByteCount)
    {
        BufferWriter writer;

        CHECK(writer.Write(uint32_t{ 42 }));
        CHECK(writer.WriteString("Eppo"));
        CHECK(writer.WriteBytes(nullptr, 0));

        CHECK(writer.IsValid());
        CHECK_EQUAL(sizeof(uint32_t) + sizeof(uint32_t) + 4, writer.GetSize());
        CHECK_EQUAL(writer.GetSize(), writer.GetOffset());
    }

    TEST(BufferReaderWriter_PrimitivesStructStringsAndBytes_RoundTripInOrder)
    {
        struct TestValue
        {
            uint32_t First;
            float Second;
        };

        constexpr uint64_t primitive = 0x123456789abcdef0;
        constexpr TestValue value{ 17, 3.5f };
        constexpr std::array<uint8_t, 4> bytes{ 4, 3, 2, 1 };

        BufferWriter sizingWriter;
        CHECK(sizingWriter.Write(primitive));
        CHECK(sizingWriter.Write(value));
        CHECK(sizingWriter.WriteString(""));
        CHECK(sizingWriter.WriteString("runtime"));
        CHECK(sizingWriter.WriteBytes(bytes.data(), bytes.size()));

        Buffer buffer(sizingWriter.GetSize());
        BufferWriter writer(buffer);
        CHECK(writer.Write(primitive));
        CHECK(writer.Write(value));
        CHECK(writer.WriteString(""));
        CHECK(writer.WriteString("runtime"));
        CHECK(writer.WriteBytes(bytes.data(), bytes.size()));
        REQUIRE CHECK(writer.IsValid());
        CHECK_EQUAL(buffer.Size, writer.GetOffset());

        BufferReader reader(buffer);
        uint64_t readPrimitive = 0;
        TestValue readValue{};
        std::array<uint8_t, 4> readBytes{};
        CHECK(reader.Read(readPrimitive));
        CHECK(reader.Read(readValue));
        CHECK_EQUAL(std::string{}, reader.ReadString());
        CHECK_EQUAL(std::string("runtime"), reader.ReadString());
        CHECK(reader.ReadBytes(readBytes.data(), readBytes.size()));
        CHECK(reader.IsValid());
        CHECK_EQUAL(primitive, readPrimitive);
        CHECK_EQUAL(value.First, readValue.First);
        CHECK_CLOSE(value.Second, readValue.Second, 0.0001f);
        CHECK_ARRAY_EQUAL(bytes.data(), readBytes.data(), bytes.size());
        CHECK_EQUAL(0, reader.GetRemaining());

        buffer.Release();
    }

    TEST(BufferWriter_InsufficientCapacity_BecomesInvalidWithoutOverflow)
    {
        Buffer buffer(sizeof(uint32_t));
        BufferWriter writer(buffer);

        CHECK(writer.Write(uint32_t{ 1 }));
        CHECK(!writer.Write(uint8_t{ 2 }));
        CHECK(!writer.IsValid());
        CHECK_EQUAL(sizeof(uint32_t), writer.GetOffset());

        buffer.Release();
    }

    TEST(BufferReader_TruncatedValues_BecomeInvalid)
    {
        const std::array<uint8_t, 3> truncatedPrimitive{};
        Buffer primitiveBuffer(const_cast<uint8_t*>(truncatedPrimitive.data()), truncatedPrimitive.size());
        BufferReader primitiveReader(primitiveBuffer);
        uint32_t primitive = 0;
        CHECK(!primitiveReader.Read(primitive));
        CHECK(!primitiveReader.IsValid());

        const uint32_t stringSize = 4;
        Buffer lengthBuffer(reinterpret_cast<uint8_t*>(const_cast<uint32_t*>(&stringSize)), sizeof(stringSize) - 1);
        BufferReader lengthReader(lengthBuffer);
        CHECK_EQUAL(std::string{}, lengthReader.ReadString());
        CHECK(!lengthReader.IsValid());

        std::array<uint8_t, sizeof(uint32_t) + 2> stringData{};
        std::memcpy(stringData.data(), &stringSize, sizeof(stringSize));
        Buffer stringBuffer(stringData.data(), stringData.size());
        BufferReader stringReader(stringBuffer);
        CHECK_EQUAL(std::string{}, stringReader.ReadString());
        CHECK(!stringReader.IsValid());

        Buffer byteBuffer(stringData.data(), 1);
        BufferReader byteReader(byteBuffer);
        std::array<uint8_t, 2> bytes{};
        CHECK(!byteReader.ReadBytes(bytes.data(), bytes.size()));
        CHECK(!byteReader.IsValid());
    }

    TEST(BufferReader_SubReader_CannotEscapeDeclaredPayload)
    {
        std::array<uint8_t, 8> bytes{ 1, 2, 3, 4, 5, 6, 7, 8 };
        Buffer buffer(bytes.data(), bytes.size());
        BufferReader reader(buffer);

        BufferReader payload = reader.ReadSubReader(4);
        uint32_t value = 0;
        CHECK(payload.Read(value));
        CHECK(payload.IsValid());
        CHECK_EQUAL(0, payload.GetRemaining());
        CHECK(!payload.Read(value));
        CHECK(!payload.IsValid());
        CHECK(reader.IsValid());
        CHECK_EQUAL(4, reader.GetOffset());
        CHECK_EQUAL(4, reader.GetRemaining());
    }

    TEST(BufferReaderWriter_ZeroLengthReads_DoNotAdvance)
    {
        BufferWriter sizingWriter;
        CHECK(sizingWriter.WriteBytes(nullptr, 0));
        CHECK_EQUAL(0, sizingWriter.GetOffset());

        Buffer buffer;
        BufferWriter writer(buffer);
        CHECK(writer.WriteBytes(nullptr, 0));
        CHECK(writer.IsValid());
        CHECK_EQUAL(0, writer.GetOffset());

        BufferReader reader(buffer);
        CHECK(reader.ReadBytes(nullptr, 0));
        CHECK(reader.IsValid());
        CHECK_EQUAL(0, reader.GetOffset());
    }
}
