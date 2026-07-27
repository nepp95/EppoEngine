#include "pch.h"
#include "Project/GameData.h"

#include "Asset/PackFormat.h"
#include "Core/Buffer/FileStreamReader.h"
#include "Core/Buffer/FileStreamWriter.h"

#include <ranges>

namespace Eppo
{
	auto GameData::Serialize(const std::filesystem::path& path) const -> bool
	{
		EP_PROFILE_FN("GameData::Serialize");

		FileStreamWriter writer(path);
		if (!writer.IsStreamGood())
			return false;

		// Header
		writer.WriteRaw<uint32_t>(PackFormat::Package.Magic);
		writer.WriteRaw<uint32_t>(PackFormat::Package.Version);
		writer.WriteString(ProjectName);
		writer.WriteRaw<uint64_t>(static_cast<uint64_t>(StartScene));

		// Asset registry (runtime assets are regenerated, not shipped)
		uint32_t registryCount = 0;
		for (const auto& metadata : AssetRegistry | std::views::values)
			registryCount += metadata.IsRuntimeAsset ? 0 : 1;

		writer.WriteRaw<uint32_t>(registryCount);
		for (const auto& [handle, metadata] : AssetRegistry)
		{
			if (metadata.IsRuntimeAsset)
				continue;

			writer.WriteRaw<uint64_t>(static_cast<uint64_t>(handle));
			writer.WriteRaw<uint8_t>(static_cast<uint8_t>(metadata.Type));
			writer.WriteString(metadata.Filepath.generic_string());
		}

		// Packed assets
		writer.WriteRaw<uint32_t>(static_cast<uint32_t>(PackedAssets.size()));
		for (const auto& [handle, packedAsset] : PackedAssets)
		{
			writer.WriteRaw<uint64_t>(static_cast<uint64_t>(handle));
			writer.WriteRaw<uint8_t>(static_cast<uint8_t>(packedAsset.Type));
			writer.WriteBuffer(packedAsset.Payload);
		}

		// Shaders
		writer.WriteRaw<uint32_t>(PackFormat::Shader.Magic);
		writer.WriteRaw<uint32_t>(PackFormat::Shader.Version);
		writer.WriteRaw<uint32_t>(static_cast<uint32_t>(PackedShaders.size()));
		for (const auto& [name, shader] : PackedShaders)
		{
			writer.WriteString(name);
			writer.WriteRaw<uint32_t>(static_cast<uint32_t>(shader.ShaderSources.size()));
			for (const auto& [type, source] : shader.ShaderSources)
			{
				writer.WriteRaw<uint16_t>(static_cast<uint16_t>(type));
				writer.WriteString(source);
			}
		}

		writer.WriteRaw<uint32_t>(static_cast<uint32_t>(PackedShaderIncludes.size()));
		for (const auto& [shaderPath, source] : PackedShaderIncludes)
		{
			writer.WriteString(shaderPath);
			writer.WriteString(source);
		}

		return writer.IsStreamGood();
	}

	auto GameData::Deserialize(const std::filesystem::path& path) -> bool
	{
		EP_PROFILE_FN("GameData::Deserialize");

		FileStreamReader reader(path);
		if (!reader.IsStreamGood())
			return false;

		// Header
		uint32_t magic = 0;
		uint32_t version = 0;
		reader.ReadRaw<uint32_t>(magic);
		reader.ReadRaw<uint32_t>(version);
		if (magic != PackFormat::Package.Magic || version != PackFormat::Package.Version)
		{
			Log::Error("'{}' is not a supported package format (magic {:#x}, version {})", path, magic, version);
			return false;
		}

		reader.ReadString(ProjectName);
		uint64_t startScene = 0;
		reader.ReadRaw<uint64_t>(startScene);
		StartScene = AssetHandle(startScene);

		// Asset registry
		uint32_t registryCount = 0;
		reader.ReadRaw<uint32_t>(registryCount);
		for (uint32_t i = 0; i < registryCount; i++)
		{
			uint64_t handle = 0;
			uint8_t type = 0;
			reader.ReadRaw<uint64_t>(handle);
			reader.ReadRaw<uint8_t>(type);
			std::string filepath;
			reader.ReadString(filepath);

			AssetRegistry[AssetHandle(handle)] = AssetMetadata{
				.Handle = AssetHandle(handle),
				.Type = static_cast<AssetType>(type),
				.Filepath = filepath,
			};
		}

		// Packed assets
		uint32_t packedCount = 0;
		reader.ReadRaw<uint32_t>(packedCount);
		for (uint32_t i = 0; i < packedCount; i++)
		{
			uint64_t handle = 0;
			uint8_t type = 0;
			reader.ReadRaw<uint64_t>(handle);
			reader.ReadRaw<uint8_t>(type);

			PackedAssetData packedAsset;
			packedAsset.Type = static_cast<AssetType>(type);
			reader.ReadBuffer(packedAsset.Payload);
			PackedAssets[AssetHandle(handle)] = std::move(packedAsset);
		}

		// Shaders
		uint32_t shaderMagic = 0;
		uint32_t shaderVersion = 0;
		reader.ReadRaw<uint32_t>(shaderMagic);
		reader.ReadRaw<uint32_t>(shaderVersion);
		if (shaderMagic != PackFormat::Shader.Magic || shaderVersion != PackFormat::Shader.Version)
		{
			Log::Error("Shader block in '{}' is not a supported format (magic {:#x}, version {})", path, shaderMagic, shaderVersion);
			return false;
		}

		uint32_t shaderCount = 0;
		reader.ReadRaw<uint32_t>(shaderCount);
		for (uint32_t i = 0; i < shaderCount; i++)
		{
			std::string name;
			reader.ReadString(name);
			uint32_t stageCount = 0;
			reader.ReadRaw<uint32_t>(stageCount);

			PackedShaderData shader;
			for (uint32_t j = 0; j < stageCount; j++)
			{
				uint16_t type = 0;
				reader.ReadRaw<uint16_t>(type);
				reader.ReadString(shader.ShaderSources[static_cast<nvrhi::ShaderType>(type)]);
			}
			PackedShaders[name] = std::move(shader);
		}

		uint32_t includeCount = 0;
		reader.ReadRaw<uint32_t>(includeCount);
		for (uint32_t i = 0; i < includeCount; i++)
		{
			std::string includePath;
			reader.ReadString(includePath);
			reader.ReadString(PackedShaderIncludes[includePath]);
		}

		// A package written before shader includes were carried runs out here rather than at the version check,
		// since its shader block is otherwise identical.
		if (!reader.IsStreamGood())
		{
			Log::Error("'{}' ended before it was fully read; it is truncated or was written by an older build.", path);
			return false;
		}

		return true;
	}
}
