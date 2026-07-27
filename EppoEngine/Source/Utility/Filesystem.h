#pragma once

#include "Core/Buffer/Buffer.h"

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

    inline auto IsDirectory(const std::filesystem::path& path) -> bool
    {
        return std::filesystem::is_directory(path);
    }

    inline auto IsEmpty(const std::filesystem::path& path) -> bool
    {
        return std::filesystem::is_empty(path);
    }

    inline auto RemoveAll(const std::filesystem::path& path) -> bool
    {
        std::filesystem::remove_all(path);
        return true;
    }

    // Copies a single file, creating parent directories as needed.
    inline auto CopyFile(const std::filesystem::path& source, const std::filesystem::path& destination, const bool overwrite) -> bool
    {
        if (const auto parent = destination.parent_path(); !parent.empty())
            std::filesystem::create_directories(parent);

        const auto options = overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none;
        return std::filesystem::copy_file(source, destination, options);
    }

    // Copies the files under source into destination for which predicate(path) is true, preserving the tree
    // when recursive.
    template<typename Predicate>
    inline auto CopyDirectory(
        const std::filesystem::path& source, const std::filesystem::path& destination, Predicate predicate, const bool recursive = true
    ) -> bool
    {
        if (recursive)
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(source))
            {
                if (!entry.is_regular_file() || !predicate(entry.path()))
                    continue;

                if (!CopyFile(entry.path(), destination / std::filesystem::relative(entry.path(), source), true))
                    return false;
            }
        }
        else
        {
            for (const auto& entry : std::filesystem::directory_iterator(source))
            {
                if (!entry.is_regular_file() || !predicate(entry.path()))
                    continue;

                if (!CopyFile(entry.path(), destination / entry.path().filename(), true))
                    return false;
            }
        }
        return true;
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

    inline auto WriteBytes(const std::filesystem::path& path, const Buffer& buffer, const bool overwrite) -> bool
    {
        return WriteBytes(path, buffer.Data, buffer.Size, overwrite);
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
