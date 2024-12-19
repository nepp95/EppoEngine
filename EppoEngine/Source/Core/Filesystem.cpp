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
		s_Data = new FilesystemData();

		s_Data->RootPath = std::filesystem::current_path();
		s_Data->AssetPath = s_Data->RootPath / "Resources";
	}

	void Filesystem::Shutdown()
	{
		delete s_Data;
	}

	const std::filesystem::path& Filesystem::GetAppRootDirectory()
	{
		return s_Data->RootPath;
	}

	const std::filesystem::path& Filesystem::GetAssetsDirectory()
	{
		return s_Data->AssetPath;
	}

	bool Filesystem::CreateDirectory(const std::filesystem::path& path)
	{
		return std::filesystem::create_directories(path);
	}

	bool Filesystem::Copy(const std::filesystem::path& from, const std::filesystem::path& to)
	{
		// Nothing to copy
		if (!Exists(from))
		{
			EPPO_ERROR("Cannot copy from '{}' because the path does not exist!", from);
			return false;
		}

		// Copy
		std::filesystem::copy(from, to);

		return true;
	}

	bool Filesystem::Move(const std::filesystem::path& from, const std::filesystem::path& to)
	{
		std::filesystem::rename(from, to);

		return true;
	}

	bool Filesystem::Rename(const std::filesystem::path& basePath, const std::string& from, const std::string& to)
	{
		std::filesystem::path fromPath = basePath / from;

		if (!Exists(fromPath))
		{
			EPPO_ERROR("Cannot rename '{}' because the path does not exist!", fromPath);
			return false;
		}

		std::filesystem::path toPath = basePath / to;
		std::filesystem::rename(fromPath, toPath);

		return true;
	}

	bool Filesystem::Exists(const std::filesystem::path& path)
	{
		return std::filesystem::exists(path);
	}

	Buffer Filesystem::ReadBytes(const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("Filesystem::ReadBytes");

		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
		if (!stream)
			return {};

		const std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		const size_t fileSize = end - stream.tellg();

		if (fileSize == 0)
			return {};

		Buffer buffer(static_cast<uint32_t>(fileSize));
		stream.read(buffer.As<char>(), fileSize);
		stream.close();

		return buffer;
	}

	std::string Filesystem::ReadText(const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("Filesystem::ReadText");

		std::string text;

		std::ifstream stream(filepath, std::ios::binary | std::ios::in);
		if (!stream)
			return text;

		stream.seekg(0, std::ios::end);

		if (const size_t size = stream.tellg(); size != -1)
		{
			text.resize(size);
			stream.seekg(0, std::ios::beg);
			stream.read(text.data(), text.size());
		}

		return text;
	}

	void Filesystem::WriteBytes(const std::filesystem::path& filepath, Buffer buffer, const bool overwrite)
	{
		EPPO_PROFILE_FUNCTION("Filesystem::WriteBytes");

		if (Exists(filepath) && !overwrite)
			return;

		std::ofstream stream(filepath, std::ios::binary);
		EPPO_ASSERT(stream)

		stream.write(buffer.As<char>(), buffer.Size);
	}

	void Filesystem::WriteBytes(const std::filesystem::path& filepath, const std::vector<uint32_t>& buffer, const bool overwrite)
	{
		EPPO_PROFILE_FUNCTION("Filesystem::WriteBytes");

		if (Exists(filepath) && !overwrite)
			return;

		std::ofstream stream(filepath, std::ios::binary);
		EPPO_ASSERT(stream)

		stream.write((char*)buffer.data(), buffer.size() * sizeof(uint32_t));
	}

	void Filesystem::WriteText(const std::filesystem::path& filepath, const std::string& text, bool overwrite)
	{
		EPPO_PROFILE_FUNCTION("Filesystem::WriteText");

		if (Exists(filepath) && !overwrite)
			return;

		std::ofstream stream;
		if (overwrite)
			stream.open(filepath, std::ios::out | std::ios::trunc);
		else
			stream.open(filepath, std::ios::out);

		EPPO_ASSERT(stream)

		stream.write(text.c_str(), text.size());
	}
}
