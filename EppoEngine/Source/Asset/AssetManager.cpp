#include "pch.h"
#include "Asset/AssetManager.h"

#include "Asset/AssetImporter.h"
#include "Project/Project.h"
#include "Utility/Json.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Eppo
{
    AssetManager::AssetManager(std::map<AssetHandle, AssetMetadata>&& assetData, std::map<AssetHandle, PackedAssetData>&& packedAssets)
        : m_AssetData(std::move(assetData)), m_PackedAssets(std::move(packedAssets)), m_UsesPackedAssets(true)
    {}

    auto AssetManager::GetAssetTypeFromPath(const std::filesystem::path& path) -> AssetType
    {
        const auto ext = path.extension().string();

        if (ext == ".gltf" || ext == ".glb")
            return AssetType::Mesh;
        if (ext == ".epscene")
            return AssetType::Scene;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr")
            return AssetType::Texture;
        if (ext == ".cs")
            return AssetType::Script;

        return AssetType::None;
    }

    auto AssetManager::CreateAsset(const std::filesystem::path& path, const Ref<Asset>& existingAsset) -> bool
    {
        EP_PROFILE_FN("AssetManager::CreateAsset");

        const AssetHandle handle = existingAsset ? existingAsset->Handle : UUID();
        const AssetMetadata metadata{
            .Handle = handle,
            .Type = GetAssetTypeFromPath(path),
            .Filepath = Project::GetAssetRelativeFilepath(path),
        };

        {
            std::scoped_lock lock(m_Mutex);

            if (m_AssetData.contains(handle))
            {
                Log::Error("Asset already contained in asset manager!");
                return false;
            }
            else
            {
                m_AssetData[handle] = metadata;
            }
        }

        SerializeAssetRegistry();

        return true;
    }

    auto AssetManager::GetOrLoadAsset(AssetHandle handle, bool async) -> Ref<Asset>
    {
        EP_PROFILE_FN("AssetManager::LoadAsset");

        std::scoped_lock lock(m_Mutex);

        // Asset already loaded
        if (m_LoadedAssets.contains(handle))
            return m_LoadedAssets.at(handle);

        Ref<Asset> asset = nullptr;

        // Create generated asset if handle is reserved
        if (auto id = static_cast<uint64_t>(handle); id < 100)
            asset = GenerateAsset(handle);

        // Create asset instance
        if (!asset && !m_AssetData.contains(handle))
        {
            Log::Error("Failed to load asset with handle '{}'", handle);
            return nullptr;
        }

        const auto& metadata = m_AssetData.at(handle);

        if (!asset)
        {
            if (async)
            {
                // EP_ASSERT(!m_LoadFutures.contains(handle));

                // m_LoadFutures[handle] = std::async(std::launch::async, [this, handle, asset, metadata]()
                //{
                //     EP_PROFILE_FN("AssetManager::LoadAsset::Lambda");

                //    // Load asset here


                //    m_LoadedComplete.insert(handle);
                //});
            }
            else
            {
                if (const auto packedAsset = m_PackedAssets.find(handle); packedAsset != m_PackedAssets.end())
                {
                    asset = AssetImporter::ImportPackedAsset(handle, metadata, packedAsset->second.Payload);
                }
                else if (m_UsesPackedAssets && metadata.Type == AssetType::Scene)
                {
                    Log::Error("Packed scene '{}' has no payload", handle);
                    return nullptr;
                }
                else
                {
                    asset = AssetImporter::ImportAsset(handle, metadata);
                }
            }
        }

        if (!asset)
            return nullptr;

        asset->Handle = handle;
        m_LoadedAssets[handle] = asset;

        return asset;
    }

    auto AssetManager::Tick() -> void {}

    auto AssetManager::HasAssetData(AssetHandle handle) const -> bool
    {
        return m_AssetData.contains(handle);
    }

    auto AssetManager::IsAssetLoaded(AssetHandle handle) const -> bool
    {
        return m_AssetData.contains(handle) && m_LoadedAssets.contains(handle);
    }

    auto AssetManager::GetMetadata(AssetHandle handle) -> AssetMetadata&
    {
        return m_AssetData.at(handle);
    }

    auto AssetManager::GetMetadata(AssetHandle handle) const -> const AssetMetadata&
    {
        return m_AssetData.at(handle);
    }

    auto AssetManager::GetHandleForPath(const std::filesystem::path& path) const -> AssetHandle
    {
        // Registry stores asset-relative paths, so normalize whatever we get.
        const auto relative = path.is_absolute() ? Project::GetAssetRelativeFilepath(path) : path;

        std::shared_lock lock(m_Mutex);

        for (const auto& [handle, metadata] : m_AssetData)
        {
            if (metadata.Filepath == relative)
                return handle;
        }

        return 0;
    }

    auto AssetManager::RemoveAsset(AssetHandle handle) -> void
    {
        {
            std::scoped_lock lock(m_Mutex);

            if (!m_AssetData.contains(handle))
            {
                Log::Warn("Tried to remove asset '{}' which is not in the registry!", handle);
                return;
            }

            m_AssetData.erase(handle);
            m_LoadedAssets.erase(handle);
        }

        SerializeAssetRegistry();
    }

    auto AssetManager::SerializeAssetRegistry() const -> void
    {
        EP_PROFILE_FN("AssetManager::SerializeAssetRegistry");

        std::shared_lock lock(m_Mutex);

        json data;
        data["Assets"] = nlohmann::json::array();
        for (const auto& [handle, metadata] : m_AssetData)
        {
            if (metadata.Filepath.empty() || metadata.IsRuntimeAsset)
                continue;

            json asset;
            asset["Handle"] = handle;
            asset["Type"] = Utils::AssetTypeToString(metadata.Type);
            asset["Filepath"] = metadata.Filepath.string();
            data["Assets"].emplace_back(asset);
        }

        lock.unlock();
        FS::WriteText(Project::GetAssetsDirectory() / "AssetRegistry.json", data.dump(4), true);
    }

    auto AssetManager::DeserializeAssetRegistry() -> bool
    {
        EP_PROFILE_FN("AssetManager::DeserializeAssetRegistry");

        const auto path = Project::GetAssetsDirectory() / "AssetRegistry.json";
        if (!FS::Exists(path))
        {
            Log::Error("Could not deserialize asset registry!");
            return false;
        }

        std::ifstream stream(path);
        json data;

        try
        {
            data = json::parse(stream);
        }
        catch (const json::exception& ex)
        {
            Log::Error("Failed to parse asset registry file '{}'!", path);
            Log::Error("Parse error: {}", ex.what());
            return false;
        }

        if (!data.contains("Assets"))
            return false;

        std::scoped_lock lock(m_Mutex);

        for (const auto& e : data["Assets"])
        {
            const AssetHandle handle = e["Handle"].get<UUID>();

            const AssetMetadata metadata{
                .Handle = handle,
                .Type = Utils::AssetTypeFromString(e["Type"].get<std::string>()),
                .Filepath = e["Filepath"].get<std::string>(),
            };

            m_AssetData[handle] = metadata;
        }

        return true;
    }

    auto AssetManager::GenerateAsset(AssetHandle handle) -> Ref<Asset>
    {
        const auto id = static_cast<uint64_t>(handle);

        if (id > 0 && id < 10)
        {
            Ref<Mesh> mesh = Mesh::GenerateMeshPrimitive(static_cast<MeshPrimitiveType>(id));

            const AssetMetadata metadata{
                .Handle = handle,
                .Type = AssetType::Mesh,
                .IsRuntimeAsset = true,
            };

            m_AssetData[handle] = metadata;

            return mesh;
        }

        return nullptr;
    }
}
