#pragma once

#include "Core/UUID.h"

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace nlohmann
{
    template<>
    struct adl_serializer<glm::vec2>
    {
        static auto to_json(json& j, const glm::vec2& v) -> void { j = json::array({ v.x, v.y }); }
        static auto from_json(const json& j, glm::vec2& v) -> void
        {
            v.x = j[0];
            v.y = j[1];
        }
    };

    template<>
    struct adl_serializer<glm::vec3>
    {
        static auto to_json(json& j, const glm::vec3& v) -> void { j = json::array({ v.x, v.y, v.z }); }
        static auto from_json(const json& j, glm::vec3& v) -> void
        {
            v.x = j[0];
            v.y = j[1];
            v.z = j[2];
        }
    };

    template<>
    struct adl_serializer<glm::vec4>
    {
        static auto to_json(json& j, const glm::vec4& v) -> void { j = json::array({ v.x, v.y, v.z, v.w }); }
        static auto from_json(const json& j, glm::vec4& v) -> void
        {
            v.x = j[0];
            v.y = j[1];
            v.z = j[2];
            v.w = j[3];
        }
    };

    template<>
    struct adl_serializer<Eppo::UUID>
    {
        static auto to_json(json& j, const Eppo::UUID& id) -> void { j = static_cast<uint64_t>(id); }
        static auto from_json(const json& j, Eppo::UUID& id) -> void { id = Eppo::UUID(j.get<uint64_t>()); }
    };
}