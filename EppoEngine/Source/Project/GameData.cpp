#include "pch.h"
#include "Project/GameData.h"

#include "Asset/PackFormat.h"

#include <fstream>
#include <ranges>

namespace Eppo
{
	namespace Utils
	{
		template<typename T>
		static auto Write(std::ostream& out, const T& value) -> void
		{
			out.write(reinterpret_cast<const char*>(&value), sizeof(T));
		}

		static auto WriteString(std::ostream& out, const std::string& value) -> void
		{
			Write(out, static_cast<uint32_t>(value.size()));
			out.write(value.data(), static_cast<std::streamsize>(value.size()));
		}

		static auto WriteBytes(std::ostream& out, const Buffer& buffer) -> void
		{
			Write(out, static_cast<uint64_t>(buffer.Size));
			out.write(reinterpret_cast<const char*>(buffer.Data), static_cast<std::streamsize>(buffer.Size));
		}

		template<typename T>
		static auto Read(std::istream& in, T& value) -> void
		{
			in.read(reinterpret_cast<char*>(&value), sizeof(T));
		}

		static auto ReadString(std::istream& in) -> std::string
		{
			uint32_t size = 0;
			Read(in, size);
			std::string value(size, '\0');
			in.read(value.data(), static_cast<std::streamsize>(size));
			return value;
		}

		static auto ReadBytes(std::istream& in) -> Buffer
		{
			uint64_t size = 0;
			Read(in, size);
			Buffer buffer(size);
			in.read(reinterpret_cast<char*>(buffer.Data), static_cast<std::streamsize>(size));
			return buffer;
		}
	}

	auto GameData::Serialize(const std::filesystem::path& path) const -> bool
	{
		EP_PROFILE_FN("GameData::Serialize");

		std::ofstream out(path, std::ios::binary);
		if (!out)
		{
			Log::Error("Failed to open file stream for path '{}'", path);
			return false;
		}

		// Header
		Utils::Write(out, PackFormat::Package.Magic);
		Utils::Write(out, PackFormat::Package.Version);
		Utils::WriteString(out, ProjectName);
		Utils::Write(out, static_cast<uint64_t>(StartScene));

		// Asset registry (runtime assets are regenerated, not shipped)
		uint32_t registryCount = 0;
		for (const auto& metadata : AssetRegistry | std::views::values)
			registryCount += metadata.IsRuntimeAsset ? 0 : 1;

		Utils::Write(out, registryCount);
		for (const auto& [handle, metadata] : AssetRegistry)
		{
			if (metadata.IsRuntimeAsset)
				continue;

			Utils::Write(out, static_cast<uint64_t>(handle));
			Utils::Write(out, static_cast<uint8_t>(metadata.Type));
			Utils::WriteString(out, metadata.Filepath.generic_string());
		}

		// Packed assets
		Utils::Write(out, static_cast<uint32_t>(PackedAssets.size()));
		for (const auto& [handle, packedAsset] : PackedAssets)
		{
			Utils::Write(out, static_cast<uint64_t>(handle));
			Utils::Write(out, static_cast<uint8_t>(packedAsset.Type));
			Utils::WriteBytes(out, packedAsset.Payload);
		}

		// Shaders
		Utils::Write(out, PackFormat::Shader.Magic);
		Utils::Write(out, PackFormat::Shader.Version);
		Utils::Write(out, static_cast<uint32_t>(PackedShaders.size()));
		for (const auto& [name, shader] : PackedShaders)
		{
			Utils::WriteString(out, name);
			Utils::Write(out, static_cast<uint32_t>(shader.ShaderSources.size()));
			for (const auto& [type, source] : shader.ShaderSources)
			{
				Utils::Write(out, static_cast<uint16_t>(type));
				Utils::WriteString(out, source);
			}
		}

		Utils::Write(out, static_cast<uint32_t>(PackedShaderIncludes.size()));
		for (const auto& [path, source] : PackedShaderIncludes)
		{
			Utils::WriteString(out, path);
			Utils::WriteString(out, source);
		}

		return static_cast<bool>(out);
	}

	auto GameData::Deserialize(const std::filesystem::path& path) -> bool
	{
		EP_PROFILE_FN("GameData::Deserialize");

		std::ifstream in(path, std::ios::binary);
		if (!in)
		{
			Log::Error("Failed to open file stream for path '{}'", path);
			return false;
		}

		// Header
		uint32_t magic = 0;
		uint32_t version = 0;
		Utils::Read(in, magic);
		Utils::Read(in, version);
		if (magic != PackFormat::Package.Magic || version != PackFormat::Package.Version)
		{
			Log::Error("'{}' is not a supported package format (magic {:#x}, version {})", path, magic, version);
			return false;
		}

		ProjectName = Utils::ReadString(in);
		uint64_t startScene = 0;
		Utils::Read(in, startScene);
		StartScene = AssetHandle(startScene);

		// Asset registry
		uint32_t registryCount = 0;
		Utils::Read(in, registryCount);
		for (uint32_t i = 0; i < registryCount; i++)
		{
			uint64_t handle = 0;
			uint8_t type = 0;
			Utils::Read(in, handle);
			Utils::Read(in, type);
			const std::string filepath = Utils::ReadString(in);

			AssetRegistry[AssetHandle(handle)] = AssetMetadata{
				.Handle = AssetHandle(handle),
				.Type = static_cast<AssetType>(type),
				.Filepath = filepath,
			};
		}

		// Packed assets
		uint32_t packedCount = 0;
		Utils::Read(in, packedCount);
		for (uint32_t i = 0; i < packedCount; i++)
		{
			uint64_t handle = 0;
			uint8_t type = 0;
			Utils::Read(in, handle);
			Utils::Read(in, type);

			PackedAssetData packedAsset;
			packedAsset.Type = static_cast<AssetType>(type);
			packedAsset.Payload = Utils::ReadBytes(in);
			PackedAssets[AssetHandle(handle)] = std::move(packedAsset);
		}

		// Shaders
		uint32_t shaderMagic = 0;
		uint32_t shaderVersion = 0;
		uint32_t shaderCount = 0;
		Utils::Read(in, shaderMagic);
		Utils::Read(in, shaderVersion);
		if (shaderMagic != PackFormat::Shader.Magic || shaderVersion != PackFormat::Shader.Version)
		{
			Log::Error("Shader block in '{}' is not a supported format (magic {:#x}, version {})", path, shaderMagic, shaderVersion);
			return false;
		}

		Utils::Read(in, shaderCount);
		for (uint32_t i = 0; i < shaderCount; i++)
		{
			const std::string name = Utils::ReadString(in);
			uint32_t stageCount = 0;
			Utils::Read(in, stageCount);

			PackedShaderData shader;
			for (uint32_t j = 0; j < stageCount; j++)
			{
				uint16_t type = 0;
				Utils::Read(in, type);
				shader.ShaderSources[static_cast<nvrhi::ShaderType>(type)] = Utils::ReadString(in);
			}
			PackedShaders[name] = std::move(shader);
		}

		uint32_t includeCount = 0;
		Utils::Read(in, includeCount);
		for (uint32_t i = 0; i < includeCount; i++)
		{
			const std::string includePath = Utils::ReadString(in);
			PackedShaderIncludes[includePath] = Utils::ReadString(in);
		}

		// A package written before shader includes were carried runs out here rather than at the version check,
		// since its shader block is otherwise identical.
		if (!in)
		{
			Log::Error("'{}' ended before it was fully read; it is truncated or was written by an older build.", path);
			return false;
		}

		return true;
	}
}
