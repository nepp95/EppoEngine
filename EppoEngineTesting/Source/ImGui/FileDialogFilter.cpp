#include "Support/EppoTest.h"

#include "ImGui/FileDialog.h"

SUITE(FileDialogFilter)
{
    using Eppo::FileDialog;

    TEST(BuildFilter_SingleExtension_OmitsLabelAndGrouping)
    {
        CHECK_EQUAL(std::string(".epscene"), FileDialog::BuildFilter("EppoEngine Scene", { "epscene" }));
    }

    TEST(BuildFilter_MultipleExtensions_GroupUnderLabel)
    {
        CHECK_EQUAL(
            std::string("Importable Assets{.gltf,.glb,.png}"),
            FileDialog::BuildFilter("Importable Assets", { "gltf", "glb", "png" })
        );
    }

    TEST(BuildFilter_NoExtensions_IsEmpty)
    {
        CHECK_EQUAL(std::string(), FileDialog::BuildFilter("Anything", {}));
    }
}
