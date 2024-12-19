#pragma once

#include "Asset/AssetMetadata.h"
#include "Renderer/Mesh/Mesh.h"
#include "Scene/Scene.h"

namespace Eppo
{
	class AssetImporter
	{
	public:
		// Importing
		static Ref<Asset> ImportAsset(AssetHandle handle, const AssetMetadata& metadata);

		static Ref<Mesh> ImportMesh(AssetHandle handle, const AssetMetadata& metadata);
		static Ref<Scene> ImportScene(AssetHandle handle, const AssetMetadata& metadata);

		// Exporting
		static bool ExportScene(const Ref<Scene>& scene, const std::filesystem::path& filepath);
	};
}
