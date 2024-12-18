#include "pch.h"
#include "AssetManagerEditor.h"

#include "Asset/AssetImporter.h"
#include "Project/Project.h"

#include <yaml-cpp/yaml.h>

namespace Eppo
{
	static std::map<std::filesystem::path, AssetType> s_AssetExtensionMap =
	{
		{ ".epscene", AssetType::Scene },
		{ ".glb", AssetType::Mesh },
		{ ".gltf", AssetType::Mesh },
		{ ".jpeg", AssetType::Texture },
		{ ".jpg", AssetType::Texture },
		{ ".png", AssetType::Texture },
	};

	namespace Utils
	{
		static AssetType GetAssetTypeFromFileExtension(const std::filesystem::path& extension)
		{
			if (s_AssetExtensionMap.find(extension) == s_AssetExtensionMap.end())
			{
				EPPO_WARN("Could not find AssetType for '{}'", extension);
				return AssetType::None;
			}

			return s_AssetExtensionMap.at(extension);
		}

		static std::filesystem::path CopyAssetToAssetsDirectory(const std::filesystem::path& filepath)
		{
			const AssetType type = GetAssetTypeFromFileExtension(filepath.extension());
			std::filesystem::path destPath;

			switch (type)
			{
				case AssetType::Mesh:
				{
					destPath = Project::GetAssetsDirectory() / "Meshes";
					break;
				}

				case AssetType::Scene:
				{
					destPath = Project::GetAssetsDirectory() / "Scenes";
					break;
				}

				case AssetType::Script:
				{
					destPath = Project::GetAssetsDirectory() / "Scripts";
					break;
				}

				case AssetType::Texture:
				{
					destPath = Project::GetAssetsDirectory() / "Textures";
					break;
				}
			}

			Filesystem::Copy(filepath, destPath);

			return destPath / filepath.filename();
		}
	}

	static const auto s_NullMetadata = AssetMetadata();

	bool AssetManagerEditor::CreateAsset(const Ref<Asset> asset, const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("AssetManagerEditor::CreateAsset");

		const AssetHandle handle = asset->Handle;
		if (IsAssetHandleValid(handle))
			return false;

		if (IsAssetLoaded(handle))
			return false;

		const AssetType type = Utils::GetAssetTypeFromFileExtension(filepath.extension());
		EPPO_ASSERT(type != AssetType::None)

		AssetMetadata metadata;
		metadata.Filepath = Project::GetAssetRelativeFilepath(filepath);
		metadata.Handle = handle;
		metadata.Type = type;

		m_AssetData[handle] = metadata;
		m_Assets[handle] = asset;

		SerializeAssetRegistry();

		return true;
	}

	Ref<Asset> AssetManagerEditor::GetAsset(const AssetHandle handle)
	{
		EPPO_PROFILE_FUNCTION("AssetManagerEditor::GetAsset");

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

			asset->Handle = handle;
			m_Assets.insert_or_assign(handle, asset);
		}

		return asset;
	}

	bool AssetManagerEditor::IsAssetHandleValid(const AssetHandle handle) const
	{
		return handle != 0 && m_AssetData.find(handle) != m_AssetData.end();
	}

	bool AssetManagerEditor::IsAssetLoaded(const AssetHandle handle) const
	{
		return m_Assets.find(handle) != m_Assets.end();
	}

	AssetType AssetManagerEditor::GetAssetType(const AssetHandle handle) const
	{
		if (!IsAssetHandleValid(handle))
			return AssetType::None;

		return m_AssetData.at(handle).Type;
	}

	Ref<Asset> AssetManagerEditor::ImportAsset(const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("AssetManagerEditor::ImportAsset");

		// If the path is not inside the assets directory, we want to copy the file to the assets directory
		std::filesystem::path baseCanonical = std::filesystem::canonical(Project::GetAssetsDirectory());
		std::filesystem::path targetCanonical = std::filesystem::canonical(Project::GetAssetFilepath(filepath));

		const AssetType type = Utils::GetAssetTypeFromFileExtension(filepath.extension());
		EPPO_ASSERT(type != AssetType::None)

		std::filesystem::path newPath;
		if (std::mismatch(baseCanonical.begin(), baseCanonical.end(), targetCanonical.begin()).first != baseCanonical.end())
			newPath = Utils::CopyAssetToAssetsDirectory(targetCanonical);

		AssetMetadata metadata;
		metadata.Filepath = Project::GetAssetRelativeFilepath(newPath.empty() ? filepath : newPath);
		metadata.Type = type;

		Ref<Asset> asset = AssetImporter::ImportAsset(metadata.Handle, metadata);
		if (asset)
		{
			const AssetHandle handle;
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

	void AssetManagerEditor::SerializeAssetRegistry() const
	{
		EPPO_PROFILE_FUNCTION("AssetManagerEditor::SerializeAssetRegistry");
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
		EPPO_PROFILE_FUNCTION("AssetManagerEditor::DeserializeAssetRegistry");

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
		catch (YAML::ParserException& e)
		{
			EPPO_ERROR("Failed to load asset registry file '{}'!", assetRegistryFile);
			EPPO_ERROR("YAML Error: {}", e.what());
			return false;
		}

		for (const auto& asset : data)
		{
			std::filesystem::path filepath = asset["Filepath"].as<std::string>();
			if (!Filesystem::Exists(Project::GetAssetFilepath(filepath)))
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

			EPPO_TRACE("Asset '{}' ({}) deserialized", filepath.string(), handle);
		}

		// Since the information can have changed if a asset did not exist, we serialize it again
		SerializeAssetRegistry();

		return true;
	}
}
