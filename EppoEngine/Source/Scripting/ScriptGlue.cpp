#include "pch.h"
#include "Scripting/ScriptGlue.h"

#include "Core/Input.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scripting/ScriptEngine.h"

namespace Eppo
{
    namespace
    {
        // Called from managed via C# `delegate* unmanaged[Cdecl]`. The engine is
        // x64-only (single calling convention), so a plain function matches.
        auto LogMessage(const uint8_t level, const char* message) -> void
        {
            EP_ASSERT(level <= 3);
            switch (level)
            {
                case 0:
                {
                    Log::Trace("{}", message);
                    break;
                }

                case 1:
                {
                    Log::Info("{}", message);
                    break;
                }

                case 2:
                {
                    Log::Warn("{}", message);
                    break;
                }

                case 3:
                default:
                {
                    Log::Error("{}", message);
                    break;
                }

            }
        }

        #pragma region Core
        auto Input_IsKeyPressed(const uint16_t keyCode) -> bool
        {
            return Input::IsKeyPressed(keyCode);
        }
        #pragma endregion

        #pragma region Scene
        // Resolve a script-side entity id (its UUID) back to a live Entity via the
        // scene set on play. Returns a null Entity when no scene is active or the
        // id is unknown; callers must check it.
        auto GetEntity(const uint64_t id) -> Entity
        {
            if (!ScriptEngine::IsInitialized())
                return {};

            auto scene = ScriptEngine::Get().GetSceneContext();
            if (!scene)
            {
                Log::Error("Script internal call made with no active scene");
                return {};
            }

            return scene->GetEntityByUUID(UUID(id));
        }

        auto GetScene() -> Ref<Scene>
        {
            if (!ScriptEngine::IsInitialized())
                return {};

            auto scene = ScriptEngine::Get().GetSceneContext();
            if (!scene)
            {
                Log::Error("Script internal call made with no active scene");
                return nullptr;
            }

            return scene;
        }

        auto Entity_HasComponent(const uint64_t id, const char* typeName) -> bool
        {
            const Entity entity = GetEntity(id);
            if (!entity)
                return false;

            const std::string_view name(typeName);
            if (name == "TransformComponent")
                return entity.HasComponent<TransformComponent>();
            if (name == "MeshComponent")
                return entity.HasComponent<MeshComponent>();
            if (name == "CameraComponent")
                return entity.HasComponent<CameraComponent>();
            if (name == "PointLightComponent")
                return entity.HasComponent<PointLightComponent>();
            if (name == "ScriptComponent")
                return entity.HasComponent<ScriptComponent>();
            if (name == "RelationshipComponent")
                return entity.HasComponent<RelationshipComponent>();

            return false;
        }

        auto Entity_AddComponent(const uint64_t id, const char* typeName) -> void
        {
            Entity entity = GetEntity(id);
            if (!entity)
                return;

            const std::string_view name(typeName);
            if (name == "TransformComponent")
                entity.TryAddComponent<TransformComponent>();
            if (name == "MeshComponent")
                entity.TryAddComponent<MeshComponent>();
            if (name == "CameraComponent")
                entity.TryAddComponent<CameraComponent>();
            if (name == "PointLightComponent")
                entity.TryAddComponent<PointLightComponent>();
            if (name == "ScriptComponent")
                entity.TryAddComponent<ScriptComponent>();
            if (name == "RelationshipComponent")
                entity.TryAddComponent<RelationshipComponent>();
        }

        auto Entity_RemoveComponent(const uint64_t id, const char* typeName) -> bool
        {
            Entity entity = GetEntity(id);
            if (!entity)
                return false;

            const std::string_view name(typeName);
            if (name == "TransformComponent")
                return entity.RemoveComponent<TransformComponent>();
            if (name == "MeshComponent")
                return entity.RemoveComponent<MeshComponent>();
            if (name == "CameraComponent")
                return entity.RemoveComponent<CameraComponent>();
            if (name == "PointLightComponent")
                return entity.RemoveComponent<PointLightComponent>();
            if (name == "ScriptComponent")
                return entity.RemoveComponent<ScriptComponent>();
            if (name == "RelationshipComponent")
                return entity.RemoveComponent<RelationshipComponent>();

            return false;
        }

        auto TransformComponent_GetTranslation(const uint64_t id, glm::vec3* outTranslation) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<TransformComponent>())
                return;

            *outTranslation = entity.GetComponent<TransformComponent>().Translation;
        }

        auto TransformComponent_SetTranslation(const uint64_t id, const glm::vec3* translation) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<TransformComponent>())
                return;

            entity.GetComponent<TransformComponent>().Translation = *translation;
        }

        auto MeshComponent_GetMeshHandle(const uint64_t id) -> uint64_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<MeshComponent>())
                return 0;

            return static_cast<uint64_t>(entity.GetComponent<MeshComponent>().MeshHandle);
        }

        auto PointLightComponent_GetColor(const uint64_t id, glm::vec3* outColor) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<PointLightComponent>())
                return;

            *outColor = entity.GetComponent<PointLightComponent>().Color;
        }

        auto PointLightComponent_SetColor(const uint64_t id, glm::vec3* color) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<PointLightComponent>())
                return;

            entity.GetComponent<PointLightComponent>().Color = *color;
        }

        auto PointLightComponent_GetIntensity(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<PointLightComponent>())
                return 0.0f;

            return entity.GetComponent<PointLightComponent>().Intensity;
        }

        auto PointLightComponent_SetIntensity(const uint64_t id, const float intensity) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<PointLightComponent>())
                return;

            entity.GetComponent<PointLightComponent>().Intensity = intensity;
        }

        auto RelationshipComponent_GetParent(const uint64_t id) -> uint64_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RelationshipComponent>())
                return 0;

            return static_cast<uint64_t>(entity.GetComponent<RelationshipComponent>().Parent);
        }

        auto RelationshipComponent_SetParent(const uint64_t id, const uint64_t parent) -> void
        {
            const auto& scene = GetScene();
            const Entity entity = scene->GetEntityByUUID(id);
            if (!entity || !entity.HasComponent<RelationshipComponent>())
                return;

            scene->SetParent(entity, Entity{ static_cast<EntityHandle>(parent), scene.get() });
        }
        #pragma endregion
    }

    auto ScriptGlue::GetInternalCalls() -> std::span<const InternalCall>
    {
        static const InternalCall calls[] = {
            { "LogMessage",                        reinterpret_cast<void*>(&LogMessage) },
            { "Input_IsKeyPressed",                reinterpret_cast<void*>(&Input_IsKeyPressed) },
            { "Entity_HasComponent",               reinterpret_cast<void*>(&Entity_HasComponent)               },
            { "Entity_AddComponent",               reinterpret_cast<void*>(&Entity_AddComponent)               },
            { "Entity_RemoveComponent",            reinterpret_cast<void*>(&Entity_RemoveComponent)            },
            { "TransformComponent_GetTranslation", reinterpret_cast<void*>(&TransformComponent_GetTranslation) },
            { "TransformComponent_SetTranslation", reinterpret_cast<void*>(&TransformComponent_SetTranslation) },
            { "MeshComponent_GetMeshHandle",       reinterpret_cast<void*>(&MeshComponent_GetMeshHandle)       },
            { "PointLightComponent_GetColor",      reinterpret_cast<void*>(&PointLightComponent_GetColor)      },
            { "PointLightComponent_SetColor",      reinterpret_cast<void*>(&PointLightComponent_SetColor)      },
            { "PointLightComponent_GetIntensity",  reinterpret_cast<void*>(&PointLightComponent_GetIntensity)  },
            { "PointLightComponent_SetIntensity",  reinterpret_cast<void*>(&PointLightComponent_SetIntensity)  },
            { "RelationshipComponent_GetParent",   reinterpret_cast<void*>(&RelationshipComponent_GetParent)   },
            { "RelationshipComponent_SetParent",   reinterpret_cast<void*>(&RelationshipComponent_SetParent)   },
        };

        return calls;
    }
}
