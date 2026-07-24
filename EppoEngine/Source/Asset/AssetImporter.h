#pragma once

#include "Asset/AssetMetadata.h"
#include "Renderer/Mesh.h"
#include "Scene/Scene.h"

namespace Eppo
{
	using importFn = std::function<Ref<Asset>(AssetHandle, const AssetMetadata&)>;
    using importPackedFn = std::function<Ref<Asset>(AssetHandle, BufferReader&)>;
	using exportFn = std::function<bool(const Ref<Asset>&, const std::filesystem::path&)>;

	class AssetImporter
	{
	public:
		static auto ImportAsset(AssetHandle handle, const AssetMetadata& metadata) -> Ref<Asset>;
		static auto ImportAsset(AssetHandle handle, AssetType type, BufferReader& reader) -> Ref<Asset>;

		static auto ImportMesh(AssetHandle handle, const AssetMetadata& metadata) -> Ref<Mesh>;
		static auto ImportScene(AssetHandle handle, const AssetMetadata& metadata) -> Ref<Scene>;
	    static auto ImportPackedMesh(AssetHandle handle, BufferReader& reader) -> Ref<Mesh>;
		static auto ImportPackedScene(AssetHandle handle, BufferReader& reader) -> Ref<Scene>;

		static auto ExportAsset(const Ref<Asset>& asset, const std::filesystem::path& path) -> bool;

		static auto ExportMesh(const Ref<Asset>& asset, const std::filesystem::path& path) -> bool;
		static auto ExportScene(const Ref<Asset>& asset, const std::filesystem::path& path) -> bool;

	private:
		static std::map<AssetType, importFn> s_AssetImportFns;
		static std::map<AssetType, importPackedFn> s_AssetImportPackedFns;
		static std::map<AssetType, exportFn> s_AssetExportFns;
	};
}
