#pragma once

#include <string>

namespace Eppo
{
	enum class AssetType : uint8_t
	{
		None = 0,
		Mesh,
		Scene,
		Script,
		Texture
	};

	namespace Utils
	{
		inline AssetType AssetTypeFromString(const std::string& assetType)
		{
			if (assetType == "None")		return AssetType::None;
			if (assetType == "Mesh")		return AssetType::Mesh;
			if (assetType == "Scene")		return AssetType::Scene;
			if (assetType == "Script")		return AssetType::Script;
			if (assetType == "Texture")		return AssetType::Texture;

			// TODO: Used in editor content browser, move there?
			if (assetType == "Meshes")		return AssetType::Mesh;
			if (assetType == "Scenes")		return AssetType::Scene;
			if (assetType == "Scripts")		return AssetType::Script;
			if (assetType == "Textures")	return AssetType::Texture;

			EPPO_ASSERT(false)
			return AssetType::None;
		}

		inline std::string AssetTypeToString(const AssetType type)
		{
			switch (type)
			{
				case AssetType::None:		return "None";
				case AssetType::Mesh:		return "Mesh";
				case AssetType::Scene:		return "Scene";
				case AssetType::Script:		return "Script";
				case AssetType::Texture:	return "Texture";
			}

			EPPO_ASSERT(false)
			return "None";
		}

		inline const char* AssetTypeToImGuiPayloadType(const AssetType type)
		{
			switch (type)
			{
				case AssetType::None:		return "None";
				case AssetType::Mesh:		return "MESH_ASSET";
				case AssetType::Scene:		return "SCENE_ASSET";
				case AssetType::Script:		return "SCRIPT_ASSET";
				case AssetType::Texture:	return "TEXTURE_ASSET";
			}

			EPPO_ASSERT(false)
			return "None";
		}
	}
}
