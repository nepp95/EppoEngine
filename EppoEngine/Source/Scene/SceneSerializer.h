#pragma once

#include "Scene/Entity.h"
#include "Scene/Scene.h"

// TODO: Possibly remove to remove dependency from runtime
#include <nlohmann/json.hpp>

namespace YAML
{
    class Emitter;
}

namespace Eppo
{
    class SceneSerializer
    {
    public:
        explicit SceneSerializer(const Ref<Scene>& scene);

        bool Serialize(const std::filesystem::path& filepath);
        [[nodiscard]] bool Deserialize(const std::filesystem::path& filepath) const;

    private:
        void SerializeEntity(nlohmann::json& data, Entity entity);

    private:
        Ref<Scene> m_SceneContext;
    };
}
