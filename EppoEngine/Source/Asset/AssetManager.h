#pragma once

#include "Asset/Asset.h"
#include "Asset/AssetMetadata.h"

#include <future>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

namespace Eppo
{
	class AssetManager
	{
	public:
		// Create an asset from a file on disk
		auto CreateAsset(const std::filesystem::path& path, const Ref<Asset>& existingAsset = nullptr) -> bool;

		// Get or load an asset of which we know the metadata
		auto GetOrLoadAsset(AssetHandle handle, bool async = false) -> Ref<Asset>;

		template<typename T>
			requires(std::derived_from<T, Asset>)
		auto GetOrLoadAsset(AssetHandle handle, bool async = false) -> Ref<T>
		{
			return std::static_pointer_cast<T>(GetOrLoadAsset(handle, async));
		}

		auto Tick() -> void;

		[[nodiscard]] auto HasAssetData(AssetHandle handle) const -> bool;
		[[nodiscard]] auto IsAssetLoaded(AssetHandle handle) const -> bool;
		[[nodiscard]] auto GetMetadata(AssetHandle handle) const -> const AssetMetadata&;

		// Reverse lookup used by the content browser to tell whether a file on disk
		// is already a registered asset. Accepts absolute or asset-relative paths.
		// Returns a null handle (0) when the file is not registered.
		[[nodiscard]] auto GetHandleForPath(const std::filesystem::path& path) const -> AssetHandle;

		// Drop an asset from the registry (does not touch the file on disk) and
		// re-serialize. Used when the content browser deletes a registered asset.
		auto RemoveAsset(AssetHandle handle) -> void;

		// Point an existing asset at a new file location (after a move/rename on
		// disk) and re-serialize. No-op for unknown handles.
		auto UpdateAssetPath(AssetHandle handle, const std::filesystem::path& newPath) -> void;

		[[nodiscard]] auto GetAssetRegistry() const -> const std::map<AssetHandle, AssetMetadata>& { return m_AssetData; };

		// Deduce an asset type purely from a file's extension. Shared by CreateAsset
		// and the content browser (for picking icons on unregistered files).
		[[nodiscard]] static auto GetAssetTypeFromPath(const std::filesystem::path& path) -> AssetType;
		auto SerializeAssetRegistry() const -> void;
		auto DeserializeAssetRegistry() -> bool;

	private:
		auto GenerateAsset(AssetHandle handle) -> Ref<Asset>;

	private:
		std::map<AssetHandle, AssetMetadata> m_AssetData;
		std::unordered_map<AssetHandle, Ref<Asset>> m_LoadedAssets;
		mutable std::shared_mutex m_Mutex;
	};
}