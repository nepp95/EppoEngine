#include "pch.h"
#include "AssetImporter.h"

#include "Asset/AssetManager.h"
#include "Scene/SceneSerializer.h"
#include "Project/Project.h"

#include <map>

namespace Eppo
{
	using fn = std::function<Ref<Asset>(AssetHandle, const AssetMetadata&)>;
	static const std::map<AssetType, fn> s_AssetImportFunctions =
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
		EPPO_PROFILE_FUNCTION("AssetImporter::ImportMesh");

		Ref<Mesh> mesh = CreateRef<Mesh>(Project::GetAssetFilepath(metadata.Filepath));

		return mesh;
	}

	Ref<Scene> AssetImporter::ImportScene(AssetHandle handle, const AssetMetadata& metadata)
	{
		EPPO_PROFILE_FUNCTION("AssetImporter::ImportScene");

		Ref<Scene> scene = CreateRef<Scene>();
		SceneSerializer serializer(scene);
		serializer.Deserialize(Project::GetAssetFilepath(metadata.Filepath));

		return scene;
	}

	bool AssetImporter::ExportScene(Ref<Scene> scene, const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("AssetImporter::ExportScene");

		SceneSerializer serializer(scene);
		if (!serializer.Serialize(filepath))
			return false;

		if (!AssetManager::IsAssetHandleValid(scene->Handle) && !AssetManager::IsAssetLoaded(scene->Handle))
			return AssetManager::CreateAsset(scene, filepath);
		
		return true;
	}
}
