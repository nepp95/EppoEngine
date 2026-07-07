#include "Support/EppoTest.h"
#include "Support/TempDir.h"

#include "Utility/Filesystem.h"

#include <vector>

using namespace Eppo;
using Testing::TempDir;

// FS wraps std::filesystem with the engine's logging + overwrite conventions.
// Every case scopes its I/O to a TempDir so nothing leaks into the working tree.
SUITE(Core)
{
    TEST(FilesystemTextRoundTrip)
    {
        // Newline-free content only: WriteText opens in text mode while ReadText
        // reads binary + file_size, so embedded '\n' does not round-trip byte-for
        // byte on Windows (tracked as an engine bug). Exact-byte guarantees are
        // covered by FilesystemBytesRoundTrip instead.
        const TempDir dir;
        const auto path = dir.File("note.txt");
        const std::string content = "hello eppo";

        CHECK(FS::WriteText(path, content, true));
        CHECK(FS::Exists(path));
        CHECK_EQUAL(content, FS::ReadText(path));
    }

    TEST(FilesystemBytesRoundTrip)
    {
        const TempDir dir;
        const auto path = dir.File("blob.bin");
        const std::vector<char> bytes = { 0x00, 0x01, 0x02, static_cast<char>(0xFF) };

        CHECK(FS::WriteBytes(path, bytes, true));
        const std::vector<char> read = FS::ReadBytes(path);
        CHECK_EQUAL(bytes.size(), read.size());
        CHECK(bytes == read);
    }

    TEST(FilesystemWriteWithoutOverwriteIsRefused)
    {
        const TempDir dir;
        const auto path = dir.File("keep.txt");

        CHECK(FS::WriteText(path, "original", true));
        // overwrite=false must leave the existing file untouched.
        CHECK(!FS::WriteText(path, "replacement", false));
        CHECK_EQUAL(std::string("original"), FS::ReadText(path));
    }

    TEST(FilesystemExistsReflectsReality)
    {
        const TempDir dir;
        const auto path = dir.File("ghost.txt");

        CHECK(!FS::Exists(path));
        CHECK(FS::WriteText(path, "x", true));
        CHECK(FS::Exists(path));
    }

    TEST(FilesystemCreateDirectory)
    {
        const TempDir dir;
        const auto nested = dir.File("a") / "b" / "c";

        CHECK(FS::CreateDirectory(nested));
        CHECK(FS::Exists(nested));
    }

    TEST(FilesystemReadMissingFileReturnsEmpty)
    {
        const TempDir dir;
        // Missing file: ReadBytes/ReadText log an error and return empty rather
        // than throwing. This exercises the null-logger guard in the runner.
        CHECK(FS::ReadBytes(dir.File("nope.bin")).empty());
        CHECK(FS::ReadText(dir.File("nope.txt")).empty());
    }
}
