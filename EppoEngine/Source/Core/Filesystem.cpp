#include "pch.h"
#include "Filesystem.h"

namespace Eppo
{
	struct FilesystemData
	{
		std::filesystem::path RootPath;
	};

	FilesystemData* s_Data;
	
	void Filesystem::Init()
	{
		s_Data = new FilesystemData();

		s_Data->RootPath = std::filesystem::current_path();
	}

	void Filesystem::Shutdown()
	{
		delete s_Data;
	}

	bool Filesystem::Exists(const std::filesystem::path& path)
	{
		return std::filesystem::exists(path);
	}

	Buffer Filesystem::ReadBytes(const std::filesystem::path& filepath)
	{
		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
		if (!stream)
			return {};

		std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		uint32_t fileSize = end - stream.tellg();

		if (fileSize == 0)
			return {};

		Buffer buffer(fileSize);
		stream.read(buffer.As<char>(), fileSize);
		stream.close();

		return buffer;
	}

	void Filesystem::WriteBytes(const std::filesystem::path& filepath, Buffer buffer, bool overwrite)
	{
		if (Exists(filepath) && !overwrite)
			return;

		std::ofstream stream(filepath, std::ios::binary);
		EPPO_ASSERT(stream);

		stream.write(buffer.As<char>(), buffer.Size);
		stream.close();
	}

	void Filesystem::WriteBytes(const std::filesystem::path& filepath, const std::vector<uint32_t>& buffer, bool overwrite)
	{
		if (Exists(filepath) && !overwrite)
			return;

		std::ofstream stream(filepath, std::ios::binary);
		EPPO_ASSERT(stream);

		stream.write((char*)buffer.data(), buffer.size() * sizeof(uint32_t));
		stream.close();
	}
}
