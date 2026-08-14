#include "pch.h"
#include "SceneSerializer.h"

#include "Core/Filesystem.h"
#include "Scripting/ScriptClass.h"
#include "Scripting/ScriptEngine.h"
#include "Utility/Json.h"

namespace Eppo
{
#define WRITE_SCRIPT_FIELD(FieldType, Type)                                                                                                \
    case ScriptFieldType::FieldType:                                                                                                       \
        fieldJson["Data"] = scriptField.GetValue<Type>();                                                                                               \
        break

#define READ_SCRIPT_FIELD(FieldType, Type)                                                                                                 \
    case ScriptFieldType::FieldType:                                                                                                       \
    {                                                                                                                                      \
        Type data = scriptField["Data"].get<Type>();                                                                                        \
        fieldInstance.SetValue(data);                                                                                                      \
        break;                                                                                                                             \
    }

    SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
        : m_SceneContext(scene)
    {}

    bool SceneSerializer::Serialize(const std::filesystem::path& filepath)
    {
        EPPO_PROFILE_FUNCTION("SceneSerializer:Serialize");

        std::string sceneName = filepath.stem().string();

        EPPO_INFO("Serializing scene '{}' ({})", sceneName, m_SceneContext->Handle);

        nlohmann::json data;

        data["Scene"]["Name"] = sceneName;
        auto entities = nlohmann::json::array();

        m_SceneContext->m_Registry.sort<IDComponent>([](const auto& lhs, const auto& rhs) { return lhs.ID < rhs.ID; });
        for (const auto view = m_SceneContext->m_Registry.view<IDComponent>(); const auto e : view)
        {
            const Entity entity(e, m_SceneContext.get());
            if (!entity)
                continue;

            SerializeEntity(entities, entity);
        }

        data["Scene"]["Entities"] = entities;

        Filesystem::WriteText(filepath, data.dump(4));

        return true;
    }

    bool SceneSerializer::Deserialize(const std::filesystem::path& filepath) const
    {
        EPPO_PROFILE_FUNCTION("SceneSerializer:Deserialize");

        std::ifstream stream(filepath);
        nlohmann::json data;

        try
        {
            data = nlohmann::json::parse(stream);
        }
        catch (nlohmann::json::exception& e)
        {
            EPPO_ERROR("Failed to load scene file '{}'!", filepath);
            EPPO_ERROR("Parse Error: {}", e.what());
            return false;
        }

        auto sceneName = data["Scene"]["Name"].get<std::string>();
        EPPO_INFO("Deserializing scene '{}'", sceneName);

        auto& entities = data["Scene"]["Entities"];

        if (entities.empty())
        {
            EPPO_WARN("Scene '{}' has no entities, are you sure this is correct?", sceneName);
            return true;
        }

        for (auto& entity : entities)
        {
            if (!entity.contains("IDComponent"))
            {
                EPPO_WARN("Entity did not have a unique identifier, skipping...");
                continue;
            }

            UUID uuid = entity["IDComponent"]["UUID"].get<UUID>();

            if (!entity.contains("TagComponent"))
                EPPO_WARN("Entity with ID '{}' did not have a tag. A tag is always recommended for debugging purposes!", uuid);

            std::string tag = entity["TagComponent"]["Tag"].get<std::string>();

            Entity newEntity = m_SceneContext->CreateEntityWithUUID(uuid, tag);
            EPPO_INFO("Deserializing entity '{}' ({})", tag, uuid);

            if (entity.contains("TransformComponent"))
            {
                auto& c = entity["TransformComponent"];
                auto& nc = newEntity.GetComponent<TransformComponent>();
                nc.Translation = c["Translation"].get<glm::vec3>();
                nc.Rotation = c["Rotation"].get<glm::vec3>();
                nc.Scale = c["Scale"].get<glm::vec3>();
            }

            if (entity.contains("SpriteComponent"))
            {
                auto& c = entity["SpriteComponent"];
                auto& nc = newEntity.AddComponent<SpriteComponent>();
                nc.Color = c["Color"].get<glm::vec4>();
                nc.TextureHandle = c["TextureHandle"].get<UUID>();
            }

            if (entity.contains("MeshComponent"))
            {
                auto& c = entity["MeshComponent"];
                auto& [meshHandle] = newEntity.AddComponent<MeshComponent>();
                meshHandle = c["MeshHandle"].get<UUID>();
            }

            if (entity.contains("DirectionalLightComponent"))
            {
                auto& c = entity["DirectionalLightComponent"];
                auto& [direction, albedoColor, ambientColor, specularColor] = newEntity.AddComponent<DirectionalLightComponent>();
                direction = c["Direction"].get<glm::vec3>();
                albedoColor = c["Albedo"].get<glm::vec4>();
                ambientColor = c["Ambient"].get<glm::vec4>();
                specularColor = c["Specular"].get<glm::vec4>();
            }

            if (entity.contains("ScriptComponent"))
            {
                auto& c = entity["ScriptComponent"];
                auto& [className] = newEntity.AddComponent<ScriptComponent>();
                className = c["ClassName"].get<std::string>();

                if (auto& scriptFields = c["Fields"])
                {
                    Ref<ScriptClass> entityClass = ScriptEngine::GetEntityClass(className);
                    EPPO_ASSERT(entityClass);

                    const auto& fields = entityClass->GetFields();
                    auto& entityFields = ScriptEngine::GetScriptFieldMap(uuid);

                    for (auto scriptField : scriptFields)
                    {
                        auto name = scriptField["Name"].get<std::string>();
                        auto typeString = scriptField["Type"].get<std::string>();
                        ScriptFieldType type = Utils::ScriptFieldTypeFromString(typeString);

                        ScriptFieldInstance& fieldInstance = entityFields[name];

                        if (fields.find(name) == fields.end())
                        {
                            EPPO_ERROR("Mono field not found!");
                            continue;
                        }

                        fieldInstance.Field = fields.at(name);

                        switch (type)
                        {
                            READ_SCRIPT_FIELD(Float, float)
                            READ_SCRIPT_FIELD(Double, double)
                            READ_SCRIPT_FIELD(Bool, bool)
                            READ_SCRIPT_FIELD(Char, int8_t)
                            READ_SCRIPT_FIELD(Int16, int16_t)
                            READ_SCRIPT_FIELD(Int32, int32_t)
                            READ_SCRIPT_FIELD(Int64, int64_t)
                            READ_SCRIPT_FIELD(Byte, uint8_t)
                            READ_SCRIPT_FIELD(UInt16, uint16_t)
                            READ_SCRIPT_FIELD(UInt32, uint32_t)
                            READ_SCRIPT_FIELD(UInt64, uint64_t)
                            READ_SCRIPT_FIELD(Vector2, glm::vec2)
                            READ_SCRIPT_FIELD(Vector3, glm::vec3)
                            READ_SCRIPT_FIELD(Vector4, glm::vec4)
                            READ_SCRIPT_FIELD(Entity, uint64_t)
                        }
                    }
                }
            }

            if (entity.contains("RigidBodyComponent"))
            {
                auto& c = entity["RigidBodyComponent"];
                auto& rbc = newEntity.AddComponent<RigidBodyComponent>();
                rbc.IsActive = c["IsActive"].get<bool>();
                rbc.Mass = c["Mass"].get<float>();
            }

            if (entity.contains("CameraComponent"))
            {
                auto& c = entity["CameraComponent"];
                auto& [camera] = newEntity.AddComponent<CameraComponent>();
                camera.SetProjectionType(static_cast<ProjectionType>(c["ProjectionType"].get<int>()));
                camera.SetPerspectiveFov(c["PerspectiveFov"].get<float>());
                camera.SetPerspectiveNearClip(c["PerspectiveNearClip"].get<float>());
                camera.SetPerspectiveFarClip(c["PerspectiveFarClip"].get<float>());
                camera.SetOrthographicSize(c["OrthographicSize"].get<float>());
                camera.SetOrthographicNearClip(c["OrthographicNearClip"].get<float>());
                camera.SetOrthographicFarClip(c["OrthographicFarClip"].get<float>());
            }

            if (entity.contains("PointLightComponent"))
            {
                auto& c = entity["PointLightComponent"];
                auto& [color] = newEntity.AddComponent<PointLightComponent>();
                color = c["Color"].get<glm::vec4>();
            }
        }

        return true;
    }

    void SceneSerializer::SerializeEntity(nlohmann::json& data, Entity entity)
    {
        EPPO_ASSERT(entity.HasComponent<IDComponent>() && entity.HasComponent<TagComponent>());

        EPPO_INFO("Serializing entity '{}' ({})", entity.GetName(), entity.GetUUID());

        uint64_t handle = static_cast<uint64_t>(entity.GetUUID());

        nlohmann::json e;

        if (entity.HasComponent<IDComponent>())
        {
            e["IDComponent"]["UUID"] = handle;
        }

        if (entity.HasComponent<TagComponent>())
        {
            e["TagComponent"]["Tag"] = entity.GetName();
        }

        if (entity.HasComponent<TransformComponent>())
        {
            const auto& c = entity.GetComponent<TransformComponent>();

            e["TransformComponent"]["Translation"] = c.Translation;
            e["TransformComponent"]["Rotation"] = c.Rotation;
            e["TransformComponent"]["Scale"] = c.Scale;
        }

        if (entity.HasComponent<SpriteComponent>())
        {
            const auto& c = entity.GetComponent<SpriteComponent>();
            e["SpriteComponent"]["Color"] = c.Color;
            e["SpriteComponent"]["TextureHandle"] = c.TextureHandle;
        }

        if (entity.HasComponent<MeshComponent>())
        {
            const auto& [meshHandle] = entity.GetComponent<MeshComponent>();
            e["MeshComponent"]["MeshHandle"] = meshHandle;
        }

        if (entity.HasComponent<DirectionalLightComponent>())
        {
            const auto& [direction, albedoColor, ambientColor, specularColor] = entity.GetComponent<DirectionalLightComponent>();
            e["DirectionalLightComponent"]["Direction"] = direction;
            e["DirectionalLightComponent"]["Albedo"] = albedoColor;
            e["DirectionalLightComponent"]["Ambient"] = ambientColor;
            e["DirectionalLightComponent"]["Specular"] = specularColor;
        }

        if (entity.HasComponent<ScriptComponent>())
        {
            const auto& [className] = entity.GetComponent<ScriptComponent>();
            e["ScriptComponent"]["ClassName"] = className;

            // Fields
            if (const auto entityClass = ScriptEngine::GetEntityClass(className))
            {
                if (const auto& fields = entityClass->GetFields(); !fields.empty())
                {
                    auto& f = e["ScriptComponent"]["Fields"];
                    nlohmann::json fieldsArr = nlohmann::json::array();

                    auto& entityFields = ScriptEngine::GetScriptFieldMap(entity.GetUUID());
                    
                    for (const auto& [name, field] : fields)
                    {
                        if (!entityFields.contains(name))
                            continue;

                        nlohmann::json fieldJson;
                        fieldJson["Name"] = name;
                        fieldJson["Type"] = Utils::ScriptFieldTypeToString(field.Type);

                        ScriptFieldInstance& scriptField = entityFields.at(name);

                        switch (field.Type)
                        {
                            WRITE_SCRIPT_FIELD(Float, float);
                            WRITE_SCRIPT_FIELD(Double, double);
                            WRITE_SCRIPT_FIELD(Bool, bool);
                            WRITE_SCRIPT_FIELD(Char, int8_t);
                            WRITE_SCRIPT_FIELD(Int16, int16_t);
                            WRITE_SCRIPT_FIELD(Int32, int32_t);
                            WRITE_SCRIPT_FIELD(Int64, int64_t);
                            WRITE_SCRIPT_FIELD(Byte, uint8_t);
                            WRITE_SCRIPT_FIELD(UInt16, uint16_t);
                            WRITE_SCRIPT_FIELD(UInt32, uint32_t);
                            WRITE_SCRIPT_FIELD(UInt64, uint64_t);
                            WRITE_SCRIPT_FIELD(Vector2, glm::vec2);
                            WRITE_SCRIPT_FIELD(Vector3, glm::vec3);
                            WRITE_SCRIPT_FIELD(Vector4, glm::vec4);
                            WRITE_SCRIPT_FIELD(Entity, UUID);
                        }
                        fieldsArr.emplace_back(fieldJson);
                    }
                    f = fieldsArr;
                }
            }
        }

        if (entity.HasComponent<RigidBodyComponent>())
        {
            const auto& c = entity.GetComponent<RigidBodyComponent>();
            e["RigidBodyComponent"]["IsActive"] = c.IsActive;
            e["RigidBodyComponent"]["Mass"] = c.Mass;
        }

        if (entity.HasComponent<CameraComponent>())
        {
            const auto& [camera] = entity.GetComponent<CameraComponent>();
            const auto& cc = camera;

            e["CameraComponent"]["ProjectionType"] = static_cast<int>(cc.GetProjectionType());
            e["CameraComponent"]["PerspectiveFov"] = cc.GetPerspectiveFov();
            e["CameraComponent"]["PerspectiveNearClip"] = cc.GetPerspectiveNearClip();
            e["CameraComponent"]["PerspectiveFarClip"] = cc.GetPerspectiveFarClip();
            e["CameraComponent"]["OrthographicSize"] = cc.GetOrthographicSize();
            e["CameraComponent"]["OrthographicNearClip"] = cc.GetOrthographicNearClip();
            e["CameraComponent"]["OrthographicFarClip"] = cc.GetOrthographicFarClip();
        }

        if (entity.HasComponent<PointLightComponent>())
        {
            const auto& [color] = entity.GetComponent<PointLightComponent>();
            e["PointLightComponent"]["Color"] = color;
        }

        data.emplace_back(e);
    }
}
