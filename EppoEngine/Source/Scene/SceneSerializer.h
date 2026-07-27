#pragma once

#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scripting/ScriptFieldStorage.h"

#include <nlohmann/json_fwd.hpp>

namespace Eppo
{
    class SceneSerializer
    {
    public:
        explicit SceneSerializer(const Ref<Scene>& scene);

        auto Serialize(const std::filesystem::path& path) const -> bool;
        auto Deserialize(const std::filesystem::path& path) const -> bool;

        // Deserializes a scene straight from its .epscene bytes (a packed scene payload).
        auto Deserialize(const Buffer& buffer) const -> bool;

        static auto ConsumeRelationshipRepairNotices() -> std::vector<std::string>;

    private:
        auto DeserializeScene(const nlohmann::json& data) const -> bool;
        auto SerializeEntity(nlohmann::json& data, Entity entity) const -> void;
        [[nodiscard]] auto FindScriptFields(UUID entityId) const -> const ScriptFieldMap*;
        [[nodiscard]] auto GetScriptFields(UUID entityId) const -> ScriptFieldMap*;

        // Rebuilds sparse relationship links and detaches missing or cyclic parents.
        auto RepairRelationships(const std::string& sceneName) const -> void;

    private:
        Ref<Scene> m_SceneContext = nullptr;
    };
}
