#include "pch.h"
#include "Filesystem.h"

namespace Eppo
{
	struct FilesystemData
	{
		std::filesystem::path RootPath;
		std::filesystem::path AssetPath;
	};

	FilesystemData* s_Data;
	
	void Filesystem::Init()
	{
		EPPO_PROFILE_FUNCTION("Filesystem::Init");

		s_Data = new FilesystemData();

		s_Data->RootPath = std::filesystem::current_path();
		s_Data->AssetPath = s_Data->RootPath / "Resources";
	}

	void Filesystem::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("Filesystem::Shutdown");

		delete s_Data;
	}

	const std::filesystem::path& Filesystem::GetAssetsDirectory()
	{
		return s_Data->AssetPath;
	}

	bool Filesystem::Exists(const std::filesystem::path& path)
	{
		EPPO_PROFILE_FUNCTION("Filesystem::ReadBytes");

		return std::filesystem::exists(path);
	}

	Buffer Filesystem::ReadBytes(const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("Filesystem::ReadBytes");

		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
		if (!stream)
			return {};

		std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		size_t fileSize = end - stream.tellg();

		if (fileSize == 0)
			return {};

		Buffer buffer((uint32_t)fileSize);
		stream.read(buffer.As<char>(), fileSize);
		stream.close();

		return buffer;
	}

	void Filesystem::WriteBytes(const std::filesystem::path& filepath, Buffer buffer, bool overwrite)
	{
		EPPO_PROFILE_FUNCTION("Filesystem::WriteBytes");

		if (Exists(filepath) && !overwrite)
			return;

		std::ofstream stream(filepath, std::ios::binary);
		EPPO_ASSERT(stream);

		stream.write(buffer.As<char>(), buffer.Size);
		stream.close();
	}

	void Filesystem::WriteBytes(const std::filesystem::path& filepath, const std::vector<uint32_t>& buffer, bool overwrite)
	{
		EPPO_PROFILE_FUNCTION("Filesystem::WriteBytes");

		if (Exists(filepath) && !overwrite)
			return;

		std::ofstream stream(filepath, std::ios::binary);
		EPPO_ASSERT(stream);

		stream.write((char*)buffer.data(), buffer.size() * sizeof(uint32_t));
		stream.close();
	}
}
