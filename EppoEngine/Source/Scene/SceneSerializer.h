#pragma once

#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scripting/ScriptFieldStorage.h"

#include <nlohmann/json_fwd.hpp>

namespace Eppo
{
	struct SceneSerializerOptions
	{
		ScriptFieldStorage* ScriptFields = nullptr;
		bool CollectRelationshipRepairNotices = true;
	};

	class SceneSerializer
	{
	public:
		explicit SceneSerializer(const Ref<Scene>& scene, SceneSerializerOptions options = {});

		auto Serialize(const std::filesystem::path& path) const -> bool;
		auto Deserialize(const std::filesystem::path& path) const -> bool;
		auto Serialize(BufferWriter& writer) const -> bool;
		auto Deserialize(BufferReader& reader) const -> bool;
		static auto ConsumeRelationshipRepairNotices() -> std::vector<std::string>;

	private:
		auto SerializeEntity(nlohmann::json& data, Entity entity) const -> void;
		[[nodiscard]] auto FindScriptFields(UUID entityId) const -> const ScriptFieldMap*;
		[[nodiscard]] auto GetScriptFields(UUID entityId) const -> ScriptFieldMap*;

		// Rebuilds sparse relationship links and detaches missing or cyclic parents.
		auto RepairRelationships(const std::string& sceneName) const -> void;

	private:
		Ref<Scene> m_SceneContext = nullptr;
		SceneSerializerOptions m_Options;
	};
}
