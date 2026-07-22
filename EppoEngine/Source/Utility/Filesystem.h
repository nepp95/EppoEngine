#pragma once

#include <filesystem>
#include <fstream>

namespace Eppo::FS
{
	inline auto CreateDir(const std::filesystem::path& path) -> bool
	{
		std::error_code error;
		std::filesystem::create_directories(path, error);
		if (error)
		{
			Log::Error("Failed to create directory '{}': {}", path, error.message());
			return false;
		}
		return true;
	}

	inline auto Exists(const std::filesystem::path& path) -> bool
	{
		return std::filesystem::exists(path);
	}

	inline auto IsDirectory(const std::filesystem::path& path) -> bool
	{
		std::error_code error;
		return std::filesystem::is_directory(path, error) && !error;
	}

	inline auto IsEmpty(const std::filesystem::path& path) -> bool
	{
		std::error_code error;
		return std::filesystem::is_empty(path, error) && !error;
	}

	inline auto RemoveAll(const std::filesystem::path& path) -> bool
	{
		std::error_code error;
		std::filesystem::remove_all(path, error);
		if (error)
		{
			Log::Error("Failed to remove '{}': {}", path, error.message());
			return false;
		}
		return true;
	}

	// Copies a single file, creating parent directories as needed. Non-throwing: logs and returns false on failure.
	inline auto CopyFile(const std::filesystem::path& source, const std::filesystem::path& destination, const bool overwrite) -> bool
	{
		std::error_code error;
		if (const auto parent = destination.parent_path(); !parent.empty())
		{
			std::filesystem::create_directories(parent, error);
			if (error)
			{
				Log::Error("Failed to create directory '{}': {}", parent, error.message());
				return false;
			}
		}

		const auto options = overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none;
		std::filesystem::copy_file(source, destination, options, error);
		if (error)
		{
			Log::Error("Failed to copy '{}' to '{}': {}", source, destination, error.message());
			return false;
		}
		return true;
	}

	// Copies the files under source into destination for which predicate(path) is true, preserving the tree
	// when recursive. Non-throwing.
	template<typename Predicate>
	inline auto CopyDirectory(const std::filesystem::path& source, const std::filesystem::path& destination, Predicate predicate,
		const bool recursive = true) -> bool
	{
		std::error_code error;
		if (recursive)
		{
			for (std::filesystem::recursive_directory_iterator it(source, error), end; !error && it != end; it.increment(error))
			{
				if (!it->is_regular_file() || !predicate(it->path()))
					continue;

				const auto relative = std::filesystem::relative(it->path(), source, error);
				if (error || !CopyFile(it->path(), destination / relative, true))
					return false;
			}
		}
		else
		{
			for (std::filesystem::directory_iterator it(source, error), end; !error && it != end; it.increment(error))
			{
				if (!it->is_regular_file() || !predicate(it->path()))
					continue;

				if (!CopyFile(it->path(), destination / it->path().filename(), true))
					return false;
			}
		}
		return !error;
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
	auto GetExecutablePath() -> std::filesystem::path;
	auto GetExecutableDirectory() -> std::filesystem::path;
	auto ConfigureWritableDirectory(const std::filesystem::path& path) -> bool;
	auto GetWritableDirectory() -> std::filesystem::path;
	auto GetUserStateDirectory(const std::string& applicationName) -> std::filesystem::path;

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
	
	auto GetShaderCacheDirectory() -> std::filesystem::path;

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
