#include "pch.h"
#include "Platform/FileDialog.h"

namespace Eppo
{
    // TODO: Yep... Gtk/Zenity/Own makings?
    std::filesystem::path FileDialog::OpenFile(const char* filter)
    {
		EPPO_PROFILE_FUNCTION("FileDialog::OpenFile");

        return {};
    }

    std::filesystem::path FileDialog::SaveFile(const char* filter)
    {
		EPPO_PROFILE_FUNCTION("FileDialog::SaveFile");

        return {};
    }
}
