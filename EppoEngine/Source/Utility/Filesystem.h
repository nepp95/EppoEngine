#pragma once

#include <filesystem>
#include <fstream>

namespace Eppo::FS
{
	inline auto CreateDir(const std::filesystem::path& path) -> bool
	{
		return std::filesystem::create_directories(path);
	}

	inline auto Exists(const std::filesystem::path& path) -> bool
	{
		return std::filesystem::exists(path);
	}

	inline auto Copy(const std::filesystem::path& from, const std::filesystem::path& to) -> bool
	{
		if (!Exists(from))
		{
			Log::Error("Failed to copy from '{}' because the path does not exist!", from);
			return false;
		}

		std::filesystem::copy(from, to);
		return true;
	}

	inline auto Move(const std::filesystem::path& from, const std::filesystem::path& to) -> bool
	{
		if (!Exists(from))
		{
			Log::Error("Failed to copy from '{}' because the path does not exist!", from);
			return false;
		}

		std::filesystem::rename(from, to);
		return true;
	}

	// Absolute directory of the running executable. Defined in Filesystem.cpp so the
	// platform headers it needs don't leak through this widely-included header.
	auto GetExecutableDirectory() -> std::filesystem::path;

	inline auto GetRootDirectory() -> std::filesystem::path
	{
		// Resolve engine data (Resources/, runtimeconfig.json, EppoScriptCore.dll)
		// relative to the executable, not the working directory — the latter differs
		// per launcher (CLion sets it to the output dir, Visual Studio does not),
		// which left scripting unable to find its managed core.
		return GetExecutableDirectory();
	}

	inline auto GetResourcesDirectory() -> std::filesystem::path
	{
		return GetRootDirectory() / "Resources";
	}
	
	inline auto GetShaderCacheDirectory() -> std::filesystem::path
	{
		const std::filesystem::path cacheDir = GetResourcesDirectory() / "Shaders" / "Cache";

		if (!Exists(cacheDir))
			CreateDir(cacheDir);

		return cacheDir;
	}

	inline auto ReadBytes(const std::filesystem::path& path) -> std::vector<char>
	{
		// Open file stream
		std::ifstream in(path, std::ios::binary);
		if (!in)
		{
			Log::Error("Failed to open file stream for path '{}'", path);
			return {};
		}

		// Get file size
		const auto fileSize = std::filesystem::file_size(path);
		if (fileSize == 0)
		{
			Log::Error("Failed to read bytes because file size was zero for path '{}'", path);
			return {};
		}

		std::vector<char> bytes(fileSize);
		in.read(bytes.data(), fileSize);

		return bytes;
	}

	inline auto WriteBytes(const std::filesystem::path& path, const std::vector<char>& bytes, const bool overwrite) -> bool
	{
		if (Exists(path) && !overwrite)
			return false;

		std::ofstream out(path, std::ios::binary);
		if (!out)
		{
			Log::Error("Failed to open file stream for path '{}'", path);
			return false;
		}

		out.write(bytes.data(), bytes.size());

		return true;
	}

	inline auto WriteBytes(const std::filesystem::path& path, const void* data, size_t size, const bool overwrite) -> bool
	{
		if (Exists(path) && !overwrite)
			return false;

		std::ofstream out(path, std::ios::binary);
		if (!out)
		{
			Log::Error("Failed to open file stream for path '{}'", path);
			return false;
		}

		out.write(static_cast<const char*>(data), size);

		return true;
	}

	inline auto ReadText(const std::filesystem::path& path) -> std::string
	{
		// Open file stream
		std::ifstream in(path, std::ios::binary);
		if (!in)
		{
			Log::Error("Failed to open file stream for path '{}'", path);
			return {};
		}

		// Get file size
		const auto fileSize = std::filesystem::file_size(path);
		if (fileSize == 0)
		{
			Log::Error("Failed to read text because file size was zero for path '{}'", path);
			return {};
		}

		std::string text;
		text.resize(fileSize);
		in.read(text.data(), text.size());

		return text;
	}

	inline auto WriteText(const std::filesystem::path& path, const std::string& text, const bool overwrite) -> bool
	{
		if (Exists(path) && !overwrite)
			return false;

		std::ofstream out(path);
		if (!out)
		{
			Log::Error("Failed to open file stream for path '{}'", path);
			return false;
		}

		out.write(text.c_str(), text.size());

		return true;
	}
}