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
            .Filepath = Project::GetAssetRelativeFilepath(path).lexically_normal(),
        };

        {
            std::scoped_lock lock(m_Mutex);

            for (const auto& [existingHandle, existingMetadata] : m_AssetData)
            {
                if (existingMetadata.Filepath.lexically_normal() == metadata.Filepath)
                {
                    Log::Error("Asset path '{}' is already registered as '{}'", metadata.Filepath, existingHandle);
                    return false;
                }
            }

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

    auto AssetManager::GetOrLoadAsset(AssetHandle handle) -> Ref<Asset>
    {
        EP_PROFILE_FN("AssetManager::GetOrLoadAsset");

        const auto caller = std::this_thread::get_id();
        const auto rawHandle = static_cast<uint64_t>(handle);
        const bool reserved = rawHandle > 0 && rawHandle < 11;

        Ref<ImportState> state;
        AssetMetadata metadata;

        {
            std::unique_lock lock(m_Mutex);

            if (const auto it = m_LoadedAssets.find(handle); it != m_LoadedAssets.end())
                return it->second;

            if (!reserved)
            {
                const auto it = m_AssetData.find(handle);
                if (it == m_AssetData.end())
                {
                    Log::Error("Failed to load unregistered asset '{}'!", handle);
                    return nullptr;
                }

                metadata = it->second;
            }

            if (const auto it = m_ImportStates.find(handle); it != m_ImportStates.end())
            {
                state = it->second;
                auto owner = state->Owner;

                while (true)
                {
                    if (owner == caller)
                    {
                        Log::Error("Cyclic import dependency at asset '{}'!", handle);
                        return nullptr;
                    }

                    const auto waiting = m_WaitingImports.find(owner);
                    if (waiting == m_WaitingImports.end())
                        break;

                    const auto next = m_ImportStates.find(waiting->second);
                    if (next == m_ImportStates.end())
                        break;

                    owner = next->second->Owner;
                }

                m_WaitingImports[caller] = handle;
                state->Changed.wait(
                    lock,
                    [&state]() -> bool
                    {
                        return state->Complete;
                    }
                );
                m_WaitingImports.erase(caller);
                return state->Result;
            }
            state = Ref<ImportState>::Create();
            state->Owner = caller;
            m_ImportStates.emplace(handle, state);
        }

        Ref<Asset> result;
        try
        {
            if (reserved)
                result = GenerateAsset(handle);
            else if (const auto packed = m_PackedAssets.find(handle); packed != m_PackedAssets.end())
                result = AssetImporter::ImportPackedAsset(handle, metadata, packed->second.Payload);
            else if (m_UsesPackedAssets && metadata.Type == AssetType::Scene)
                Log::Error("Packed scene '{}' has no payload!", handle);
            else
                result = AssetImporter::ImportAsset(handle, metadata);
        }
        catch (const std::exception& error)
        {
            Log::Error("Import of '{}' failed: {}", handle, error.what());
        }
        catch (...)
        {
            Log::Error("Import of '{}' failed with an unknown exception!", handle);
        }

        {
            std::unique_lock lock(m_Mutex);

            const auto current = m_ImportStates.find(handle);
            if (current == m_ImportStates.end() || current->second != state)
                return nullptr;

            if (result && m_AssetData.contains(handle))
            {
                result->Handle = handle;
                m_LoadedAssets[handle] = result;
                state->Result = result;
            }

            state->Complete = true;
            m_ImportStates.erase(current);
        }
        state->Changed.notify_all();
        return state->Result;
    }

    auto AssetManager::Tick() -> void {}

    auto AssetManager::HasAssetData(AssetHandle handle) const -> bool
    {
        std::shared_lock lock(m_Mutex);
        return m_AssetData.contains(handle);
    }

    auto AssetManager::IsAssetLoaded(AssetHandle handle) const -> bool
    {
        std::shared_lock lock(m_Mutex);
        return m_LoadedAssets.contains(handle);
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
        const auto relative = path.is_absolute() ? Project::GetAssetRelativeFilepath(path).lexically_normal() : path;

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

            if (const auto import = m_ImportStates.find(handle); import != m_ImportStates.end())
            {
                auto state = import->second;
                state->Complete = true;
                state->Result = nullptr;
                m_ImportStates.erase(import);
                state->Changed.notify_all();
            }
        }

        SerializeAssetRegistry();
    }

    auto AssetManager::GetPlaceholderAsset(AssetType type) -> Ref<Asset>
    {
        // Reserved id's listed in UUID.cpp
        switch (type)
        {
            case AssetType::Mesh:
            {
                return GetOrLoadAsset(static_cast<uint64_t>(MeshPrimitiveType::Cube)).As<Mesh>();
                break;
            }

            case AssetType::Scene:
            {
                EP_ASSERT(false);
                break;
            }

            case AssetType::Script:
            {
                EP_ASSERT(false);
                break;
            }

            case AssetType::Texture:
            {
                return GetOrLoadAsset(10).As<Image>();
                break;
            }
        }

        return nullptr;
    }

    auto AssetManager::SerializeAssetRegistry() const -> void
    {
        EP_PROFILE_FN("AssetManager::SerializeAssetRegistry");

        std::scoped_lock writeLock(m_RegistryWriteMutex);
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

            {
                std::scoped_lock lock(m_Mutex);
                m_AssetData[handle] = metadata;
            }

            return mesh;
        }

        if (id == 10)
        {
            Ref<Image> image = Image::GenerateFallbackImage();

            const AssetMetadata metadata{
                .Handle = handle,
                .Type = AssetType::Texture,
                .IsRuntimeAsset = true,
            };

            {
                std::scoped_lock lock(m_Mutex);
                m_AssetData[handle] = metadata;
            }

            return image;
        }

        return nullptr;
    }
}
