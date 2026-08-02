#include "Support/EppoTest.h"
#include "Support/TempDir.h"

#include "Project/Project.h"
#include "Utility/Filesystem.h"

using namespace Eppo;

namespace Eppo
{
    namespace
    {
        class ScopedWorkingDirectory final
        {
        public:
            explicit ScopedWorkingDirectory(const std::filesystem::path& path)
                : m_OriginalPath(std::filesystem::current_path())
            {
                std::filesystem::current_path(path);
            }

            ~ScopedWorkingDirectory()
            {
                std::error_code error;
                std::filesystem::current_path(m_OriginalPath, error);
            }

        private:
            std::filesystem::path m_OriginalPath;
        };
    }
}

SUITE(Core)
{
    TEST(Filesystem_ResourceAndProjectDirectoriesFollowWorkingDirectory)
    {
        const auto executableDirectory = FS::GetExecutableDirectory();
        const Testing::TempDir directory;
        const ScopedWorkingDirectory workingDirectory(directory.Path());

        const auto resourcesDirectory = FS::GetResourcesDirectory();
        const auto projectsDirectory = Project::GetProjectsDirectory();
        const auto executableDirectoryAfterChange = FS::GetExecutableDirectory();

        CHECK_EQUAL((directory.Path() / "Resources").string(), resourcesDirectory.string());
        CHECK_EQUAL((directory.Path() / "Projects").string(), projectsDirectory.string());
        CHECK_EQUAL(executableDirectory.string(), executableDirectoryAfterChange.string());
    }

    TEST(Filesystem_ConfiguredWritableDirectoryOwnsLogsAndShaderCache)
    {
        const Testing::TempDir directory;
        const auto writableDirectory = directory.File("Writable");

        REQUIRE CHECK(FS::ConfigureWritableDirectory(writableDirectory));
        CHECK_EQUAL(writableDirectory.lexically_normal().string(), FS::GetWritableDirectory().string());
        CHECK_EQUAL((writableDirectory / "ShaderCache").string(), FS::GetShaderCacheDirectory().string());
        CHECK(FS::Exists(writableDirectory));
        CHECK(FS::Exists(writableDirectory / "ShaderCache"));

        CHECK(FS::ConfigureWritableDirectory({}));
        CHECK_EQUAL(std::filesystem::current_path().string(), FS::GetWritableDirectory().string());
        CHECK_EQUAL((FS::GetResourcesDirectory() / "Shaders" / "Cache").string(), FS::GetShaderCacheDirectory().string());
    }
}
