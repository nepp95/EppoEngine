#include "TestSupport/EppoTest.h"

#include "ImGui/FileDialog.h"

using Eppo::FileDialog;

TEST(FileDialogFilter, BuildFilter_SingleExtension_OmitsLabelAndGrouping)
{
    EXPECT_EQ(std::string(".epscene"), FileDialog::BuildFilter("EppoEngine Scene", { "epscene" }));
}

TEST(FileDialogFilter, BuildFilter_MultipleExtensions_GroupUnderLabel)
{
    EXPECT_EQ(
        std::string("Importable Assets{.gltf,.glb,.png}"), FileDialog::BuildFilter("Importable Assets", { "gltf", "glb", "png" })
    );
}

TEST(FileDialogFilter, BuildFilter_NoExtensions_IsEmpty)
{
    EXPECT_EQ(std::string(), FileDialog::BuildFilter("Anything", {}));
}
