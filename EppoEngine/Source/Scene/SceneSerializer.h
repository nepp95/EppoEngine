#pragma once

#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <nlohmann/json_fwd.hpp>

namespace Eppo
{
	class SceneSerializer
	{
	public:
        explicit SceneSerializer(const Ref<Scene>& scene);

		auto Serialize(const std::filesystem::path& path) const -> bool;
		auto Deserialize(const std::filesystem::path& path) const -> bool;

	private:
		auto SerializeEntity(nlohmann::json& data, Entity entity) const -> void;

	private:
		Ref<Scene> m_SceneContext = nullptr;
	};
}