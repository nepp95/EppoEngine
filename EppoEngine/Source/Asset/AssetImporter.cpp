#include "pch.h"
#include "Asset/AssetImporter.h"

#include "Asset/AssetManager.h"
#include "Scene/SceneSerializer.h"
#include "Project/Project.h"

namespace Eppo
{
	std::map<AssetType, importFn> AssetImporter::s_AssetImportFns = {
		{ AssetType::Mesh, ImportMesh },
		{ AssetType::Scene, ImportScene },
	};

    std::map<AssetType, importPackedFn> AssetImporter::s_AssetImportPackedFns = {
        { AssetType::Scene, ImportPackedScene },
    };

	std::map<AssetType, exportFn> AssetImporter::s_AssetExportFns = {
		{ AssetType::Mesh, ExportMesh },
		{ AssetType::Scene, ExportScene },
	};

	auto AssetImporter::ImportAsset(const AssetHandle handle, const AssetMetadata& metadata) -> Ref<Asset>
	{
		EP_PROFILE_FN("AssetImporter::ImportAsset");

		if (!s_AssetImportFns.contains(metadata.Type))
		{
			Log::Error("No importer available for asset type: {}", Utils::AssetTypeToString(metadata.Type));
			return nullptr;
		}

		return s_AssetImportFns.at(metadata.Type)(handle, metadata);
	}

	auto AssetImporter::ImportAsset(const AssetHandle handle, const AssetType type, BufferReader& reader) -> Ref<Asset>
	{
	    EP_PROFILE_FN("AssetImporter::ImportAsset");

	    if (!s_AssetImportPackedFns.contains(type))
	    {
	        Log::Error("No packed importer available for asset type: {}", Utils::AssetTypeToString(type));
	        return nullptr;
	    }

		return s_AssetImportPackedFns.at(type)(handle, reader);
	}

	auto AssetImporter::ImportMesh(const AssetHandle handle, const AssetMetadata& metadata) -> Ref<Mesh>
	{
		EP_PROFILE_FN("AssetImporter::ImportMesh");

		const auto path = Project::GetAssetFilepath(metadata.Filepath);
		if (!FS::Exists(path))
		{
			Log::Error("Cannot import mesh '{}': file does not exist", path);
			return nullptr;
		}

		Ref<Mesh> mesh = CreateRef<Mesh>(path.string());
		if (!mesh->IsValid())
		{
			// Caching an unrenderable mesh would hide the failure until draw time.
			Log::Error("Cannot import mesh '{}': the file could not be parsed", path);
			return nullptr;
		}

		// Serialized MeshComponents resolve through the registry's handle, not the
		// fresh UUID the Asset base constructed.
		mesh->Handle = handle;

		return mesh;
	}

	auto AssetImporter::ImportScene(const AssetHandle handle, const AssetMetadata& metadata) -> Ref<Scene>
	{
		EP_PROFILE_FN("AssetImporter::ImportScene");

		Ref<Scene> scene = CreateRef<Scene>();
		scene->Handle = handle;
		SceneSerializer serializer(scene);
		if (!serializer.Deserialize(Project::GetAssetFilepath(metadata.Filepath)))
			return nullptr;

		return scene;
	}

	auto AssetImporter::ImportPackedScene(const AssetHandle handle, BufferReader& reader) -> Ref<Scene>
	{
		Ref<Scene> scene = CreateRef<Scene>();
		scene->Handle = handle;
		if (!SceneSerializer(scene).Deserialize(reader))
			return nullptr;
		return scene;
	}

	auto AssetImporter::ExportAsset(const Ref<Asset>& asset, const std::filesystem::path& path) -> bool
	{
		EP_PROFILE_FN("AssetImporter::ExportAsset");

		const auto& assetManager = Project::GetActive()->GetAssetManager();
		const auto& metadata = assetManager->GetMetadata(asset->Handle);

		if (!s_AssetExportFns.contains(metadata.Type))
		{
			Log::Error("No exporter available for type: {}", Utils::AssetTypeToString(metadata.Type));
			return false;
		}

		return s_AssetExportFns.at(metadata.Type)(asset, path);
	}

	auto AssetImporter::ExportMesh(const Ref<Asset>& asset, const std::filesystem::path& path) -> bool
	{
		EP_PROFILE_FN("AssetImporter::ExportMesh");

		const Ref<Mesh>& mesh = std::static_pointer_cast<Mesh>(asset);

		return false;
	}

	auto AssetImporter::ExportScene(const Ref<Asset>& asset, const std::filesystem::path& path) -> bool
	{
		EP_PROFILE_FN("AssetImporter::ExportScene");

		const Ref<Scene>& scene = std::static_pointer_cast<Scene>(asset);

		if (SceneSerializer serializer(scene); !serializer.Serialize(Project::GetAssetFilepath(path)))
			return false;

		const auto& assetManager = Project::GetActive()->GetAssetManager();
		if (!assetManager->HasAssetData(scene->Handle))
		{
			if (assetManager->CreateAsset(path, scene))
				assetManager->GetOrLoadAsset(scene->Handle);
			else
				return false;
		}

		return true;
	}
}
