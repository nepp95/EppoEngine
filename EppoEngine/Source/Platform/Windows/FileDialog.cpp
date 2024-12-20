#include "pch.h"

#include "Platform/FileDialog.h"
#include "Renderer/RendererContext.h"

#include <commdlg.h>
#include <glfw/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw/glfw3native.h>

namespace Eppo
{
	std::filesystem::path FileDialog::OpenFile(const char* filter, const std::filesystem::path& initialDir)
	{
		EPPO_PROFILE_FUNCTION("FileDialog::OpenFile");

		OPENFILENAMEA ofn;
		CHAR szFile[260]{ 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window(RendererContext::Get()->GetWindowHandle());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);

		if (initialDir.empty())
		{
			if (CHAR currentDir[256]{};
				GetCurrentDirectoryA(256, currentDir))
			{
				ofn.lpstrInitialDir = currentDir;
			}
		} else
		{
			ofn.lpstrInitialDir = initialDir.string().c_str();		
		}

		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;

		return {};
	}

	std::filesystem::path FileDialog::SaveFile(const char* filter)
	{
		EPPO_PROFILE_FUNCTION("FileDialog::SaveFile");

		OPENFILENAMEA ofn;
		CHAR szFile[260]{};
		CHAR currentDir[256]{};
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window(RendererContext::Get()->GetWindowHandle());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);

		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;

		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
		ofn.lpstrDefExt = std::strchr(filter, '\0') + 1;

		if (GetSaveFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;

		return {};
	}
}
