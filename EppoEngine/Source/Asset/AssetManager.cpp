#include "pch.h"
#include "AssetManager.h"

#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Texture.h"

#include <yaml-cpp/yaml.h>

namespace Eppo
{
	static AssetMetadata s_NullMetadata;

	AssetManager& AssetManager::Get()
	{
		static AssetManager a;
		return a;
	}

	void AssetManager::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("AssetManager::Shutdown");

		m_Assets.clear();
	}

	AssetManager::AssetManager()
	{
		EPPO_PROFILE_FUNCTION("AssetManager::AssetManager");

		LoadRegistry();
		WriteRegistry();
	}

	bool AssetManager::IsAssetLoaded(AssetHandle handle)
	{
		EPPO_PROFILE_FUNCTION("AssetManager::IsAssetLoaded");

		return m_Assets.find(handle) != m_Assets.end();
	}

	bool AssetManager::IsAssetLoaded(const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("AssetManager::IsAssetLoaded");

		AssetMetadata& metadata = GetMetadata(filepath);
		return IsAssetLoaded(metadata.Handle);
	}

	AssetMetadata& AssetManager::GetMetadata(AssetHandle handle)
	{
		EPPO_PROFILE_FUNCTION("AssetManager::GetMetadata");

		auto it = m_AssetData.find(handle);
		if (it != m_AssetData.end())
			return it->second;

		return s_NullMetadata;
	}

	AssetMetadata& AssetManager::GetMetadata(const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("AssetManager::GetMetadata");

		for (auto& [handle, metadata] : m_AssetData)
		{
			if (metadata.Filepath == filepath)
				return metadata;
		}

		return s_NullMetadata;
	}

	void AssetManager::LoadAsset(const AssetMetadata& metadata)
	{
		EPPO_PROFILE_FUNCTION("AssetManager::LoadAsset");

		if (!Filesystem::Exists(metadata.Filepath))
		{
			EPPO_ERROR("Trying to load asset which does not exists: {}", metadata.Filepath.string());
			return;
		}

		if (IsAssetLoaded(metadata.Handle))
			return;
		
		Ref<Asset> asset = nullptr;

		if (!LoadData(metadata.Filepath, asset))
		{
			EPPO_ERROR("Failed to load asset: {}", metadata.Filepath.string());
			return;
		}

		m_Assets[metadata.Handle] = asset;

		EPPO_INFO("Asset with handle '{}' has been loaded", metadata.Handle);
	}

	bool AssetManager::LoadData(const std::filesystem::path& filepath, Ref<Asset>& asset)
	{
		EPPO_PROFILE_FUNCTION("AssetManager::LoadData");

		const AssetMetadata& metadata = GetMetadata(filepath);

		switch (metadata.Type)
		{
			case AssetType::Mesh:
			{
				asset = CreateRef<Mesh>(filepath);
				asset->Handle = metadata.Handle;
				return true;
			}

			case AssetType::Texture:
			{
				asset = CreateRef<Texture>(filepath);
				asset->Handle = metadata.Handle;
				return true;
			}
		}

		EPPO_ASSERT(false);
		return false;
	}

	void AssetManager::LoadRegistry()
	{
		EPPO_PROFILE_FUNCTION("AssetManager::LoadRegistry");

		std::filesystem::path assetRegistryFile = Filesystem::GetAssetsDirectory() / "AssetRegistry.epporeg";

		if (!Filesystem::Exists(assetRegistryFile))
			return;

		EPPO_INFO("Deserializing asset registry");

		YAML::Node data;

		try
		{
			data = YAML::LoadFile(assetRegistryFile.string());
		}
		catch (YAML::ParserException e)
		{
			EPPO_ERROR("Failed to load asset registry file '{}'!", assetRegistryFile.string());
			EPPO_ERROR("YAML Error: {}", e.what());
			return;
		}

		for (auto asset : data)
		{
			std::filesystem::path filepath = asset["Filepath"].as<std::string>();
			if (!Filesystem::Exists(filepath))
			{
				EPPO_WARN("Asset with filepath '{}' has been removed from the asset registry because it does not exist!", filepath.string());
				continue;
			}

			AssetHandle handle = asset["AssetHandle"].as<uint64_t>();

			AssetMetadata metadata;
			metadata.Handle = handle;
			metadata.Type = Utils::AssetTypeFromString(asset["Type"].as<std::string>());
			metadata.Filepath = filepath;

			m_AssetData[handle] = metadata;

			LoadAsset(metadata);
		}
	}

	void AssetManager::WriteRegistry()
	{
		EPPO_PROFILE_FUNCTION("AssetManager::WriteRegistry");

		if (m_AssetData.empty())
			return;

		EPPO_INFO("Serializing asset registry");

		YAML::Emitter out;
		out << YAML::BeginSeq;

		for (const auto& [handle, metadata] : m_AssetData)
		{
			out << YAML::BeginMap;
			
			out << YAML::Key << "AssetHandle" << YAML::Value << handle;
			out << YAML::Key << "Type" << YAML::Value << Utils::AssetTypeToString(metadata.Type);
			out << YAML::Key << "Filepath" << YAML::Value << metadata.Filepath.string();

			out << YAML::EndMap;
		}

		out << YAML::EndSeq;

		std::ofstream fout(Filesystem::GetAssetsDirectory() / "AssetRegistry.epporeg", std::ios::trunc);
		fout << out.c_str();
	}
}
