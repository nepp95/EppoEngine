#pragma once

#include "Core/UUID.h"

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace nlohmann
{
    template<>
    struct adl_serializer<glm::vec2>
    {
        static void to_json(json& j, const glm::vec2& v)
        {
            j = nlohmann::json::array({ v.x, v.y });
        }

        static void from_json(const json& j, glm::vec2& v)
        {
            v.x = j[0];
            v.y = j[1];
        }
    };

    template<>
    struct adl_serializer<glm::vec3>
    {
        static void to_json(nlohmann::json& j, const glm::vec3& v)
        {
            j = nlohmann::json::array({ v.x, v.y, v.z });
        }

        static void from_json(const nlohmann::json& j, glm::vec3& v)
        {
            v.x = j[0];
            v.y = j[1];
            v.z = j[2];
        }
    };

    template<>
    struct adl_serializer<glm::vec4>
    {
        static void to_json(nlohmann::json& j, const glm::vec4& v)
        {
            j = nlohmann::json::array({ v.x, v.y, v.z, v.w });
        }

        static void from_json(const nlohmann::json& j, glm::vec4& v)
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
        static void to_json(nlohmann::json& j, const Eppo::UUID& uuid)
        {
            j = static_cast<uint64_t>(uuid);
        }

        static void from_json(const nlohmann::json& j, Eppo::UUID& uuid)
        {
            uuid = j.get<uint64_t>();
        }
    };
}

//namespace YAML
//{
//    inline Emitter& operator<<(Emitter& out, const glm::vec2& v)
//    {
//        out << YAML::Flow;
//        out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
//        return out;
//    }
//
//    inline Emitter& operator<<(Emitter& out, const glm::vec3& v)
//    {
//        out << YAML::Flow;
//        out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
//        return out;
//    }
//
//    inline Emitter& operator<<(Emitter& out, const glm::vec4& v)
//    {
//        out << YAML::Flow;
//        out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
//        return out;
//    }
//
//    template<>
//    struct convert<glm::vec2>
//    {
//        static bool decode(const Node& node, glm::vec2& v)
//        {
//            if (!node.IsSequence() || node.size() != 2)
//                return false;
//
//            v.x = node[0].as<float>();
//            v.y = node[1].as<float>();
//
//            return true;
//        }
//    };
//
//    template<>
//    struct convert<glm::vec3>
//    {
//        static bool decode(const Node& node, glm::vec3& v)
//        {
//            if (!node.IsSequence() || node.size() != 3)
//                return false;
//
//            v.x = node[0].as<float>();
//            v.y = node[1].as<float>();
//            v.z = node[2].as<float>();
//
//            return true;
//        }
//    };
//
//    template<>
//    struct convert<glm::vec4>
//    {
//        static bool decode(const Node& node, glm::vec4& v)
//        {
//            if (!node.IsSequence() || node.size() != 4)
//                return false;
//
//            v.x = node[0].as<float>();
//            v.y = node[1].as<float>();
//            v.z = node[2].as<float>();
//            v.w = node[3].as<float>();
//
//            return true;
//        }
//    };
//}
