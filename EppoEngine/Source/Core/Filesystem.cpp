#include "pch.h"
#include "Filesystem.h"

#include <efsw/efsw.hpp>

namespace Eppo
{
    class FileUpdateListener final : public efsw::FileWatchListener
    {
    public:
        void handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename, efsw::Action action,
                              std::string oldFilename) override;
    };

    struct FilesystemData
    {
        std::filesystem::path RootPath;
        std::filesystem::path AssetPath;

        efsw::FileWatcher* FileWatcher;
        FileUpdateListener* FileWatcherListener;
        std::unordered_map<std::filesystem::path, std::function<void(std::filesystem::path)>> WatchFiles;
    };

    FilesystemData* s_Data;

    void FileUpdateListener::handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename,
                                              efsw::Action action, std::string oldFilename)
    {
        if (const std::filesystem::path path = dir + filename; s_Data->WatchFiles.contains(path))
            s_Data->WatchFiles[path](path);
    }

    void Filesystem::Init()
    {
        s_Data = new FilesystemData();

        s_Data->RootPath = std::filesystem::current_path();
        s_Data->AssetPath = s_Data->RootPath / "Resources";

        s_Data->FileWatcher = new efsw::FileWatcher();
        s_Data->FileWatcherListener = new FileUpdateListener();
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
        stream.read(buffer.As<char>(), static_cast<int32_t>(fileSize));
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
            stream.read(text.data(), static_cast<int32_t>(text.size()));
        }

        return text;
    }

    void Filesystem::WriteBytes(const std::filesystem::path& filepath, Buffer buffer, const bool overwrite)
    {
        EPPO_PROFILE_FUNCTION("Filesystem::WriteBytes");

        if (Exists(filepath) && !overwrite)
            return;

        std::ofstream stream(filepath, std::ios::binary);
        EPPO_ASSERT(stream);

        stream.write(buffer.As<char>(), buffer.Size);
    }

    void Filesystem::WriteBytes(const std::filesystem::path& filepath, const std::vector<uint32_t>& buffer, const bool overwrite)
    {
        EPPO_PROFILE_FUNCTION("Filesystem::WriteBytes");

        if (Exists(filepath) && !overwrite)
            return;

        std::ofstream stream(filepath, std::ios::binary);
        EPPO_ASSERT(stream);

        stream.write((char*)buffer.data(), static_cast<int32_t>(buffer.size()) * sizeof(uint32_t));
    }

    void Filesystem::WriteText(const std::filesystem::path& filepath, const std::string& text, const bool overwrite)
    {
        EPPO_PROFILE_FUNCTION("Filesystem::WriteText");

        if (Exists(filepath) && !overwrite)
            return;

        std::ofstream stream;
        if (overwrite)
            stream.open(filepath, std::ios::out | std::ios::trunc);
        else
            stream.open(filepath, std::ios::out);

        EPPO_ASSERT(stream);

        stream.write(text.c_str(), static_cast<int32_t>(text.size()));
    }

    void Filesystem::WatchFile(const std::filesystem::path& filepath, const std::function<void(std::filesystem::path)>& fn)
    {
        s_Data->FileWatcher->addWatch(filepath.parent_path().string(), s_Data->FileWatcherListener);
        s_Data->WatchFiles[filepath] = fn;
    }
}
