#include "pch.h"
#include "Platform/FileDialog.h"

namespace Eppo
{
    // TODO: Yep... Gtk/Zenity/Own makings?
    std::filesystem::path FileDialog::OpenFile(const char* filter)
    {
        return {};
    }

    std::filesystem::path FileDialog::SaveFile(const char* filter)
    {
        return {};
    }
}