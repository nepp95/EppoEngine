#include "pch.h"
#include "Project/GameData.h"

#include "Asset/PackFormat.h"

namespace Eppo
{
	namespace
	{
		auto NormalizeAssetPath(const std::filesystem::path& path) -> std::optional<std::filesystem::path>
		{
			if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory())
				return std::nullopt;

			for (const auto& part : path)
			{
				if (part == "..")
					return std::nullopt;
			}

			const auto normalized = path.lexically_normal();
			if (normalized.empty() || normalized == ".")
				return std::nullopt;
			return normalized;
		}

		auto ReadScenePayloadHandle(const PackedAssetData& packedAsset, uint64_t& handle) -> bool
		{
			constexpr size_t HandleOffset = sizeof(uint32_t) * 2;
			if (packedAsset.Payload.size() < HandleOffset + sizeof(uint64_t))
				return false;

			uint32_t magic = 0;
			uint32_t version = 0;
			std::memcpy(&magic, packedAsset.Payload.data(), sizeof(magic));
			std::memcpy(&version, packedAsset.Payload.data() + sizeof(magic), sizeof(version));
			std::memcpy(&handle, packedAsset.Payload.data() + HandleOffset, sizeof(handle));
			return magic == PackFormat::Scene.Magic && version == PackFormat::Scene.Version;
		}

		auto ValidateGameData(const GameData& data) -> bool
		{
			if (data.ProjectName.empty() || !data.StartScene)
				return false;

			for (const auto& [handle, metadata] : data.AssetRegistry)
			{
				if (!handle || handle != metadata.Handle || metadata.IsRuntimeAsset || metadata.Type == AssetType::None
					|| !NormalizeAssetPath(metadata.Filepath))
				{
					return false;
				}
			}

            if (const auto startScene = data.AssetRegistry.find(data.StartScene);
                startScene == data.AssetRegistry.end() || startScene->second.Type != AssetType::Scene)
				return false;

			for (const auto& [handle, packedAsset] : data.PackedAssets)
			{
				if (!handle || packedAsset.Type != AssetType::Scene)
					return false;

                if (const auto metadata = data.AssetRegistry.find(handle);
                    metadata == data.AssetRegistry.end() || metadata->second.Type != packedAsset.Type)
					return false;

                if (uint64_t payloadHandle = 0;
                    !ReadScenePayloadHandle(packedAsset, payloadHandle) || payloadHandle != static_cast<uint64_t>(handle))
					return false;
			}

			for (const auto& [handle, metadata] : data.AssetRegistry)
			{
				if (metadata.Type == AssetType::Scene && !data.PackedAssets.contains(handle))
					return false;
			}

			for (const auto& [name, shaderData] : data.PackedShaders)
			{
				if (name.empty() || shaderData.ShaderSources.empty())
					return false;

				for (const auto& [type, source] : shaderData.ShaderSources)
				{
					if ((type != nvrhi::ShaderType::Vertex && type != nvrhi::ShaderType::Pixel) || source.empty())
						return false;
				}
			}

			return true;
		}
	}

	auto GameData::Serialize(const std::filesystem::path& path) const -> bool
	{
	    EP_PROFILE_FN("GameData::Serialize");

		if (AssetRegistry.size() > std::numeric_limits<uint32_t>::max() || PackedAssets.size() > std::numeric_limits<uint32_t>::max())
			return false;

		GameData serialized{
		    .ProjectName = ProjectName,
		    .StartScene = StartScene,
		    .PackedAssets = PackedAssets,
		    .PackedShaders = PackedShaders,
		};

	    // Write asset metadata to game data
		for (const auto& [handle, metadata] : AssetRegistry)
		{
			if (metadata.IsRuntimeAsset)
				continue;

			const auto normalized = NormalizeAssetPath(metadata.Filepath);
			if (!normalized)
				return false;

			AssetMetadata normalizedMetadata = metadata;
			normalizedMetadata.Filepath = normalized->generic_string();
			serialized.AssetRegistry.emplace(handle, std::move(normalizedMetadata));
		}

		if (!ValidateGameData(serialized))
			return false;

	    // Write global data to buffer
	    const auto writeGlobalData = [&](BufferWriter& writer) -> bool
	    {
	        if (!writer.Write(PackFormat::Package.Magic) ||
	            !writer.Write(PackFormat::Package.Version) ||
	            !writer.WriteString(serialized.ProjectName) ||
	            !writer.Write(static_cast<uint64_t>(serialized.StartScene)))
	        {
	            return false;
	        }
	        return true;
	    };

	    // Write asset data to buffer
		const auto writeAssetData = [&](BufferWriter& writer) -> bool
		{
			if (!writer.Write(static_cast<uint32_t>(serialized.AssetRegistry.size())))
				return false;

			for (const auto& [handle, metadata] : serialized.AssetRegistry)
			{
				if (!writer.Write(static_cast<uint64_t>(handle)) || !writer.Write(static_cast<uint8_t>(metadata.Type))
					|| !writer.WriteString(metadata.Filepath.generic_string()))
				{
					return false;
				}
			}

			if (!writer.Write(static_cast<uint32_t>(serialized.PackedAssets.size())))
				return false;

			for (const auto& [handle, packedAsset] : serialized.PackedAssets)
			{
				if (!writer.Write(static_cast<uint64_t>(handle)) || !writer.Write(static_cast<uint8_t>(packedAsset.Type))
					|| !writer.Write(packedAsset.Payload.size())
					|| !writer.WriteBytes(packedAsset.Payload.data(), packedAsset.Payload.size()))
				{
					return false;
				}
			}
			return true;
		};

	    // Write shaders to game data
	    const auto writeShaderData = [&](BufferWriter& writer) -> bool
	    {
            if (!writer.Write(PackFormat::Shader.Magic) ||
                !writer.Write(PackFormat::Shader.Version))
            {
                return false;
            }

	        if (!writer.Write(static_cast<uint32_t>(serialized.PackedShaders.size())))
	            return false;

	        for (const auto& [name, shader] : serialized.PackedShaders)
	        {
	            // Name of shader
	            if (!writer.WriteString(name))
	                return false;

	            // Stage count
	            if (!writer.Write(static_cast<uint32_t>(shader.ShaderSources.size())))
	                return false;

	            // Shader sources
	            for (const auto& [type, source] : shader.ShaderSources)
	            {
	                // Shader type
	                if (!writer.Write(static_cast<uint16_t>(type)))
	                    return false;

	                // Shader source
	                if (!writer.WriteString(source))
	                    return false;
	            }
	        }

	        return true;
	    };

	    const auto writeAll = [&](BufferWriter& writer) -> bool
	    {
	        if (!writeGlobalData(writer))
	            return false;
	        if (!writeAssetData(writer))
	            return false;
	        if (!writeShaderData(writer))
	            return false;
	        return true;
	    };

		BufferWriter sizingWriter;
		if (!writeAll(sizingWriter))
			return false;

		Buffer buffer(sizingWriter.GetSize());
		BufferWriter writer(buffer);

		const bool result = writeAll(writer) && writer.IsValid() && writer.GetOffset() == buffer.Size
			&& FS::WriteBytes(path, buffer.Data, buffer.Size, true);

		buffer.Release();
		return result;
	}

	auto GameData::Deserialize(const std::filesystem::path& path) -> bool
	{
	    EP_PROFILE_FN("GameData::Deserialize");

		if (!FS::Exists(path))
			return false;

		auto bytes = FS::ReadBytes(path);
		if (bytes.empty())
			return false;

        const Buffer buffer(reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
		BufferReader reader(buffer);

	    GameData parsed;

	    // Read global data
		uint32_t pakMagic = 0;
		uint32_t pakVersion = 0;
		if (!reader.Read(pakMagic) || pakMagic != PackFormat::Package.Magic ||
		    !reader.Read(pakVersion) || pakVersion != PackFormat::Package.Version)
		{
		    return false;
		}

		parsed.ProjectName = reader.ReadString();
		if (parsed.ProjectName.empty())
			return false;

		uint64_t startScene = 0;
		if (!reader.Read(startScene) || startScene == 0)
			return false;
		parsed.StartScene = AssetHandle(startScene);

		// Read asset metadata
		uint32_t registryCount = 0;
		if (!reader.Read(registryCount) || registryCount == 0)
			return false;

		for (uint32_t i = 0; i < registryCount; i++)
		{
			uint64_t handle = 0;
			uint8_t serializedType = 0;
			if (!reader.Read(handle) || handle == 0 || !reader.Read(serializedType))
				return false;

			const auto type = static_cast<AssetType>(serializedType);
			const auto filepath = NormalizeAssetPath(reader.ReadString());
			if (!reader.IsValid() || type == AssetType::None || !filepath)
				return false;

			const AssetMetadata metadata{
				.Handle = AssetHandle(handle),
				.Type = type,
				.Filepath = filepath->generic_string(),
			};

			if (!parsed.AssetRegistry.emplace(AssetHandle(handle), metadata).second)
				return false;
		}

	    // Read asset data
		uint32_t packedCount = 0;
		if (!reader.Read(packedCount) || packedCount == 0)
			return false;

		for (uint32_t i = 0; i < packedCount; i++)
		{
			uint64_t handle = 0;
			uint8_t serializedType = 0;
			uint64_t payloadSize = 0;
			if (!reader.Read(handle) || handle == 0 || !reader.Read(serializedType) || !reader.Read(payloadSize)
				|| payloadSize > reader.GetRemaining())
				return false;

			if constexpr (sizeof(size_t) < sizeof(uint64_t))
			{
				if (payloadSize > std::numeric_limits<size_t>::max())
					return false;
			}

			const auto type = static_cast<AssetType>(serializedType);
			if (type != AssetType::Scene)
				return false;

			PackedAssetData packedAsset;
			packedAsset.Type = type;
			packedAsset.Payload.resize(payloadSize);
			if (!reader.ReadBytes(packedAsset.Payload.data(), payloadSize)
				|| !parsed.PackedAssets.emplace(AssetHandle(handle), std::move(packedAsset)).second)
				return false;
		}

	    // Read shader data
	    uint32_t shaderMagic = 0;
	    uint32_t shaderVersion = 0;
	    uint32_t shaderCount = 0;
	    if (!reader.Read(shaderMagic) || shaderMagic != PackFormat::Shader.Magic ||
	        !reader.Read(shaderVersion) || shaderVersion != PackFormat::Shader.Version ||
	        !reader.Read(shaderCount))
	    {
	        return false;
	    }

	    for (uint32_t i = 0; i < shaderCount; i++)
	    {
	        const std::string shaderName = reader.ReadString();
	        if (shaderName.empty())
	            return false;

	        uint32_t stageCount = 0;
	        if (!reader.Read(stageCount) || stageCount == 0)
	            return false;

	        PackedShaderData shaderData;
	        for (uint32_t j = 0; j < stageCount; j++)
	        {
	            uint16_t serializedType = 0;
	            if (!reader.Read(serializedType))
	                return false;

	            const auto type = static_cast<nvrhi::ShaderType>(serializedType);
	            if (type != nvrhi::ShaderType::Vertex && type != nvrhi::ShaderType::Pixel)
	                return false;

	            std::string source = reader.ReadString();
	            if (!reader.IsValid() || source.empty() || !shaderData.ShaderSources.emplace(type, std::move(source)).second)
	                return false;
	        }

	        if (!parsed.PackedShaders.emplace(shaderName, std::move(shaderData)).second)
	            return false;
	    }

	    // Verify
		if (!reader.IsValid() || reader.GetRemaining() != 0 || !ValidateGameData(parsed))
			return false;

		*this = std::move(parsed);
		return true;
	}
}
