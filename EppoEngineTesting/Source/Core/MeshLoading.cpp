#include "Support/EppoTest.h"
#include "Renderer/Mesh.h"

#include <filesystem>

using namespace Eppo;

// Mesh belongs to Renderer, but a rejected file creates no GPU resources, so these
// live in the core suite where headless CI actually runs them.
SUITE(Core)
{
    namespace
    {
        auto WriteMalformedGltf(const std::string& name) -> std::filesystem::path
        {
            const auto path = std::filesystem::temp_directory_path() / ("EppoMesh_" + name + ".gltf");
            FS::WriteText(path, "{ this is not valid glTF", true);
            return path;
        }
    }

    // Regression: tg3_model was left uninitialized, and tg3_model_free
    // unconditionally destroys model->arena_ — stack garbage on a failed parse.
    // The loader also walked the partially written model's counts.
    TEST(Mesh_MalformedFile_IsInvalidAndEmpty)
    {
        const auto path = WriteMalformedGltf("Malformed");

        const Mesh mesh(path.string());

        CHECK_EQUAL(false, mesh.IsValid());
        CHECK(mesh.GetSubmeshes().empty());

        std::filesystem::remove(path);
    }

    TEST(Mesh_MissingFile_IsInvalidAndEmpty)
    {
        const Mesh mesh((std::filesystem::temp_directory_path() / "EppoMesh_DoesNotExist.gltf").string());

        CHECK_EQUAL(false, mesh.IsValid());
        CHECK(mesh.GetSubmeshes().empty());
    }
}
