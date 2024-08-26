#include "pch.h"
#include "AssetManagerEditor.h"

#include "Asset/AssetImporter.h"
#include "Project/Project.h"

#include <yaml-cpp/yaml.h>

namespace Eppo
{
	static std::map<std::filesystem::path, AssetType> s_AssetExtensionMap =
	{
		{ ".fbx", AssetType::Mesh },
		{ ".epscene", AssetType::Scene },
		{ ".png", AssetType::Texture },
		{ ".jpg", AssetType::Texture },
		{ ".jpeg", AssetType::Texture },
	};

	namespace Utils
	{
		static AssetType GetAssetTypeFromFileExtension(const std::filesystem::path& extension)
		{
			if (s_AssetExtensionMap.find(extension) == s_AssetExtensionMap.end())
			{
				EPPO_WARN("Could not find AssetType f or '{}'", extension);
				return AssetType::None;
			}

			return s_AssetExtensionMap.at(extension);
		}
	}

	static AssetMetadata s_NullMetadata;

	Ref<Asset> AssetManagerEditor::GetAsset(AssetHandle handle)
	{
		if (!IsAssetHandleValid(handle))
			return nullptr;

		Ref<Asset> asset;
		if (IsAssetLoaded(handle))
			asset = m_Assets.at(handle);
		else
		{
			const AssetMetadata& metadata = GetMetadata(handle);
			asset = AssetImporter::ImportAsset(handle, metadata);

			if (!asset)
			{
				EPPO_ERROR("Asset importing failed!");				
			}

			m_Assets.insert_or_assign(handle, asset);
		}

		return asset;
	}

	bool AssetManagerEditor::IsAssetHandleValid(AssetHandle handle) const
	{
		return handle != 0 && m_AssetData.find(handle) != m_AssetData.end();
	}

	/*bool AssetManagerEditor::IsAssetLoaded(const std::filesystem::path& filepath)
	{
		AssetMetadata& metadata = GetMetadata(filepath);
		return AssetManager::IsAssetLoaded(metadata.Handle);
	}*/

	bool AssetManagerEditor::IsAssetLoaded(AssetHandle handle) const
	{
		return m_Assets.find(handle) != m_Assets.end();
	}

	AssetType AssetManagerEditor::GetAssetType(AssetHandle handle) const
	{
		if (!IsAssetHandleValid(handle))
			return AssetType::None;

		return m_AssetData.at(handle).Type;
	}

	Ref<Asset> AssetManagerEditor::ImportAsset(const std::filesystem::path& filepath)
	{
		AssetHandle handle;

		AssetMetadata metadata;
		metadata.Filepath = filepath;
		metadata.Type = Utils::GetAssetTypeFromFileExtension(filepath.extension());
		EPPO_ASSERT(metadata.Type != AssetType::None);

		Ref<Asset> asset = AssetImporter::ImportAsset(metadata.Handle, metadata);
		if (asset)
		{
			asset->Handle = handle;
			m_Assets[handle] = asset;
			m_AssetData[handle] = metadata;
			SerializeAssetRegistry();
		}

		return asset;
	}

	const AssetMetadata& AssetManagerEditor::GetMetadata(AssetHandle handle) const
	{
		auto it = m_AssetData.find(handle);
		if (it == m_AssetData.end())
			return s_NullMetadata;

		return it->second;
	}

	const std::filesystem::path& AssetManagerEditor::GetFilepath(AssetHandle handle) const
	{
		return GetMetadata(handle).Filepath;
	}

	void AssetManagerEditor::SerializeAssetRegistry()
	{
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

		std::ofstream fout(Project::GetAssetsDirectory() / "AssetRegistry.epporeg", std::ios::trunc);
		fout << out.c_str();
	}

	bool AssetManagerEditor::DeserializeAssetRegistry()
	{
		std::filesystem::path assetRegistryFile = Project::GetAssetsDirectory() / "AssetRegistry.epporeg";

		if (!Filesystem::Exists(assetRegistryFile))
		{
			EPPO_WARN("Asset registry file not found at {}", assetRegistryFile);
			return false;
		}

		EPPO_INFO("Deserializing asset registry");

		YAML::Node data;

		// Load the entries in the asset registry
		try
		{
			data = YAML::LoadFile(assetRegistryFile.string());
		}
		catch (YAML::ParserException e)
		{
			EPPO_ERROR("Failed to load asset registry file '{}'!", assetRegistryFile);
			EPPO_ERROR("YAML Error: {}", e.what());
			return false;
		}

		for (const auto& asset : data)
		{
			std::filesystem::path filepath = asset["Filepath"].as<std::string>();
			if (!Filesystem::Exists(filepath))
			{
				EPPO_WARN("Asset with filepath '{}' has been removed from the asset registry because it does not exist!", filepath);
				continue;
			}

			AssetHandle handle = asset["AssetHandle"].as<uint64_t>();

			AssetMetadata metadata;
			metadata.Handle = handle;
			metadata.Type = Utils::AssetTypeFromString(asset["Type"].as<std::string>());
			metadata.Filepath = filepath;

			m_AssetData[handle] = metadata;
		}

		// Since the information can have changed if a asset did not exist, we serialize it again
		SerializeAssetRegistry();
	}

	//AssetMetadata& AssetManagerEditor::GetMetadata(const std::filesystem::path& filepath)
	//{
	//	for (auto& [handle, metadata] : m_AssetData)
	//	{
	//		if (metadata.Filepath == filepath)
	//			return metadata;
	//	}

	//	return s_NullMetadata;
	//}

	//void AssetManagerEditor::DetectAssets()
	//{
	//	EPPO_INFO("Detecting new assets");

	//	for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(Project::GetAssetsDirectory()))
	//	{
	//		EPPO_TRACE(entry.path());	
	//	}
	//}

	//void AssetManagerEditor::LoadAssetsFromRegistry()
	//{
	//	for (const auto& [handle, metadata] : m_AssetData)
	//		LoadAsset(metadata);
	//}

	//void AssetManagerEditor::LoadAsset(const AssetMetadata& metadata)
	//{
	//	EPPO_INFO("Loading asset '{}' ()", metadata.Filepath.filename(), metadata.Handle);

	//	if (!Filesystem::Exists(metadata.Filepath))
	//	{
	//		EPPO_ERROR("Trying to load asset which does not exists: {}", metadata.Filepath);
	//		return;
	//	}

	//	if (AssetManager::IsAssetLoaded(metadata.Handle))
	//		return;

	//	Ref<Asset> asset = nullptr;

	//	if (!LoadData(metadata.Filepath, asset))
	//	{
	//		EPPO_ERROR("Failed to load asset: {}", metadata.Filepath);
	//		return;
	//	}

	//	m_Assets[metadata.Handle] = asset;

	//	EPPO_INFO("Asset with handle '{}' has been loaded", metadata.Handle);
	//}

	//bool AssetManagerEditor::LoadData(const std::filesystem::path& filepath, Ref<Asset>& asset)
	//{
	//	const AssetMetadata& metadata = GetMetadata(filepath);

	//	switch (metadata.Type)
	//	{
	//		case AssetType::Mesh:
	//		{
	//			asset = CreateRef<Mesh>(filepath);
	//			asset->Handle = metadata.Handle;
	//			return true;
	//		}

	//		case AssetType::Texture:
	//		{
	//			TextureSpecification textureSpec;
	//			textureSpec.Filepath = filepath;

	//			asset = CreateRef<Texture>(textureSpec);
	//			asset->Handle = metadata.Handle;
	//			return true;
	//		}
	//	}

	//	EPPO_ASSERT(false);
	//	return false;
	//}
}
