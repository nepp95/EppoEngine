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
		static auto ConsumeRelationshipRepairNotices() -> std::vector<std::string>;

	private:
		auto SerializeEntity(nlohmann::json& data, Entity entity) const -> void;

		// Rebuilds sparse relationship links and detaches missing or cyclic parents.
		auto RepairRelationships(const std::string& sceneName) const -> void;

	private:
		Ref<Scene> m_SceneContext = nullptr;
	};
}
