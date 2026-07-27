#pragma once

#include "Core/Buffer/Buffer.h"

namespace Eppo
{
    class StreamWriter;

    template<typename T>
    concept StreamSerializable = requires(StreamWriter* writer, const T& value) {
        { T::Serialize(writer, value) } -> std::same_as<bool>;
    };

    class StreamWriter
    {
    public:
        virtual ~StreamWriter() = default;

        StreamWriter(const StreamWriter&) = delete;
        auto operator=(const StreamWriter&) -> StreamWriter& = delete;
        StreamWriter(StreamWriter&&) = delete;
        auto operator=(StreamWriter&&) -> StreamWriter& = delete;

        virtual auto WriteData(const char* data, size_t size) -> bool = 0;
        auto WriteZero(uint64_t size) -> bool;
        auto WriteString(std::string_view str) -> bool;
        auto WriteBuffer(Buffer buffer) -> bool;

        [[nodiscard]] virtual auto IsStreamGood() const -> bool = 0;
        [[nodiscard]] virtual auto GetStreamPosition() const -> uint64_t = 0;

        template<typename T>
        auto WriteRaw(const T& value) -> bool
        {
            return WriteData(reinterpret_cast<const char*>(&value), sizeof(T));
        }

        template<StreamSerializable T>
        auto WriteObject(const T& value) -> bool
        {
            return T::Serialize(this, value);
        }

        template<typename Key, typename Value>
        auto WriteMap(const std::map<Key, Value>& map) -> bool
        {
            return WriteMapEntries(map);
        }

        template<typename Key, typename Value>
        auto WriteMap(const std::unordered_map<Key, Value>& map) -> bool
        {
            return WriteMapEntries(map);
        }

    protected:
        StreamWriter() = default;

    private:
        template<typename Map>
        auto WriteMapEntries(const Map& map) -> bool
        {
            if (!WriteRaw<uint32_t>(static_cast<uint32_t>(map.size())))
                return false;

            for (const auto& [key, value] : map)
            {
                if constexpr (std::is_trivially_copyable_v<typename Map::key_type>)
                {
                    if (!WriteRaw(key))
                        return false;
                }
                else
                {
                    if (!WriteObject(key))
                        return false;
                }

                if constexpr (std::is_trivially_copyable_v<typename Map::mapped_type>)
                {
                    if (!WriteRaw(value))
                        return false;
                }
                else
                {
                    if (!WriteObject(value))
                        return false;
                }
            }

            return true;
        }
    };
}
