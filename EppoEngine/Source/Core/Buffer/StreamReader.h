#pragma once

#include "Core/Buffer/Buffer.h"

namespace Eppo
{
    class StreamReader;

    template<typename T>
    concept StreamDeserializable = requires(StreamReader* reader, T& value) {
        { T::Deserialize(reader, value) } -> std::same_as<bool>;
    };

    class StreamReader
    {
    public:
        virtual ~StreamReader() = default;

        StreamReader(const StreamReader&) = delete;
        auto operator=(const StreamReader&) -> StreamReader& = delete;
        StreamReader(StreamReader&&) = delete;
        auto operator=(StreamReader&&) -> StreamReader& = delete;

        virtual auto ReadData(char* outData, size_t size) -> bool = 0;
        auto ReadString(std::string& str) -> bool;
        auto ReadBuffer(Buffer& buffer) -> bool;

        [[nodiscard]] virtual auto IsStreamGood() const -> bool = 0;
        [[nodiscard]] virtual auto GetStreamPosition() const -> uint64_t = 0;

        template<typename T>
        auto ReadRaw(T& value) -> bool
        {
            return ReadData(reinterpret_cast<char*>(&value), sizeof(T));
        }

        template<StreamDeserializable T>
        auto ReadObject(T& value) -> bool
        {
            return T::Deserialize(this, value);
        }

        template<typename Key, typename Value>
        auto ReadMap(std::map<Key, Value>& map) -> bool
        {
            return ReadMapEntries(map);
        }

        template<typename Key, typename Value>
        auto ReadMap(std::unordered_map<Key, Value>& map) -> bool
        {
            return ReadMapEntries(map);
        }

    protected:
        StreamReader() = default;

    private:
        template<typename Map>
        auto ReadMapEntries(Map& map) -> bool
        {
            uint32_t size = 0;
            if (!ReadRaw(size))
                return false;

            map.clear();
            for (uint32_t i = 0; i < size; i++)
            {
                typename Map::key_type key;

                if constexpr (std::is_trivially_copyable_v<typename Map::key_type>)
                {
                    if (!ReadRaw(key))
                        return false;
                }
                else
                {
                    if (!ReadObject(key))
                        return false;
                }

                if constexpr (std::is_trivially_copyable_v<typename Map::mapped_type>)
                {
                    if (!ReadRaw(map[key]))
                        return false;
                }
                else
                {
                    if (!ReadObject(map[key]))
                        return false;
                }
            }

            return true;
        }
    };
}
