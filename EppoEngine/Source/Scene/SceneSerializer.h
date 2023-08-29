#pragma once

#include "Scene/Entity.h"
#include "Scene/Scene.h"

namespace YAML
{
	class Emitter;
}

namespace Eppo
{
	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		bool Serialize(const std::filesystem::path& filepath);
		bool Deserialize(const std::filesystem::path& filepath);

	private:
		void SerializeEntity(YAML::Emitter& out, Entity entity);

	private:
		Ref<Scene> m_SceneContext;
	};
}
