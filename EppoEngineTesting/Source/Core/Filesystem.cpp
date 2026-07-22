#include "Support/EppoTest.h"
#include "Support/TempDir.h"

#include "Utility/Filesystem.h"

using namespace Eppo;

SUITE(Core)
{
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
