#include "pch.h"
#include "AssetImporter.h"

#include "Scene/SceneSerializer.h"
#include "Project/Project.h"

#include <map>

namespace Eppo
{
	using fn = std::function<Ref<Asset>(AssetHandle, const AssetMetadata&)>;
	static std::map<AssetType, fn> s_AssetImportFunctions =
	{
		{ AssetType::Mesh, AssetImporter::ImportMesh },
		{ AssetType::Scene, AssetImporter::ImportScene },
	};

	Ref<Asset> AssetImporter::ImportAsset(AssetHandle handle, const AssetMetadata& metadata)
	{
		if (s_AssetImportFunctions.find(metadata.Type) == s_AssetImportFunctions.end())
		{
			EPPO_ERROR("No importer available for asset type: {}", Utils::AssetTypeToString(metadata.Type));
			return nullptr;
		}

		return s_AssetImportFunctions.at(metadata.Type)(handle, metadata);
	}

	Ref<Mesh> AssetImporter::ImportMesh(AssetHandle handle, const AssetMetadata& metadata)
	{
		Ref<Mesh> mesh = CreateRef<Mesh>(Project::GetAssetFilepath(metadata.Filepath));

		return mesh;
	}

	Ref<Scene> AssetImporter::ImportScene(AssetHandle handle, const AssetMetadata& metadata)
	{
		Ref<Scene> scene = CreateRef<Scene>();
		SceneSerializer serializer(scene);
		serializer.Deserialize(Project::GetAssetFilepath(metadata.Filepath));

		return scene;
	}

	bool AssetImporter::ExportScene(Ref<Scene> scene, const std::filesystem::path& filepath)
	{
		SceneSerializer serializer(scene);
		return serializer.Serialize(filepath);
	}
}
