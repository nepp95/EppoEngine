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

TEST(Core, Filesystem_ResourceAndProjectDirectoriesFollowWorkingDirectory)
{
    const auto executableDirectory = FS::GetExecutableDirectory();
    const Testing::TempDir directory;
    const ScopedWorkingDirectory workingDirectory(directory.Path());

    const auto resourcesDirectory = FS::GetResourcesDirectory();
    const auto projectsDirectory = Project::GetProjectsDirectory();
    const auto executableDirectoryAfterChange = FS::GetExecutableDirectory();

    EXPECT_EQ((directory.Path() / "Resources").string(), resourcesDirectory.string());
    EXPECT_EQ((directory.Path() / "Projects").string(), projectsDirectory.string());
    EXPECT_EQ(executableDirectory.string(), executableDirectoryAfterChange.string());
}

TEST(Core, Filesystem_ConfiguredWritableDirectoryOwnsLogsAndShaderCache)
{
    const Testing::TempDir directory;
    const auto writableDirectory = directory.File("Writable");

    EP_REQUIRE(FS::ConfigureWritableDirectory(writableDirectory));
    EXPECT_EQ(writableDirectory.lexically_normal().string(), FS::GetWritableDirectory().string());
    EXPECT_EQ((writableDirectory / "ShaderCache").string(), FS::GetShaderCacheDirectory().string());
    EXPECT_TRUE(FS::Exists(writableDirectory));
    EXPECT_TRUE(FS::Exists(writableDirectory / "ShaderCache"));

    EXPECT_TRUE(FS::ConfigureWritableDirectory({}));
    EXPECT_EQ(std::filesystem::current_path().string(), FS::GetWritableDirectory().string());
    EXPECT_EQ((FS::GetResourcesDirectory() / "Shaders" / "Cache").string(), FS::GetShaderCacheDirectory().string());
}
