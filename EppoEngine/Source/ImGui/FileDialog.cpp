#include "pch.h"
#include "FileDialog.h"

namespace Eppo
{
	auto FileDialog::OpenFile(const std::vector<nfdfilteritem_t>& filters, const std::filesystem::path& initialDir) -> std::filesystem::path
	{
		NFD::UniquePath nfdPath = nullptr;
		auto result = NFD::OpenDialog(nfdPath, filters.data(), static_cast<uint32_t>(filters.size()), initialDir.empty() ? nullptr : initialDir.string().c_str());

		std::filesystem::path outPath = {};
		if (result == NFD_OKAY)
			outPath = nfdPath.get();
		else if (result == NFD_ERROR)
			Log::Error("NFD Failed: {}", NFD::GetError());

		return outPath;
	}

	auto FileDialog::SaveFile(const std::vector<nfdfilteritem_t>& filters, const std::filesystem::path& initialDir) -> std::filesystem::path
	{
		NFD::UniquePath nfdPath = nullptr;
		auto result = NFD::SaveDialog(nfdPath, filters.data(), static_cast<uint32_t>(filters.size()), initialDir.empty() ? nullptr : initialDir.string().c_str());

		std::filesystem::path outPath = {};
		if (result == NFD_OKAY)
			outPath = nfdPath.get();
		else if (result == NFD_ERROR)
			Log::Error("NFD Failed: {}", NFD::GetError());

		return outPath;
	}

	auto FileDialog::OpenFolder(const std::filesystem::path& initialDir) -> std::filesystem::path
	{
		NFD::UniquePath nfdPath = nullptr;
		const auto result = NFD::PickFolder(nfdPath, initialDir.empty() ? nullptr : initialDir.string().c_str());

		std::filesystem::path outPath;
		if (result == NFD_OKAY)
			outPath = nfdPath.get();
		else if (result == NFD_ERROR)
			Log::Error("NFD Failed: {}", NFD::GetError());
		return outPath;
	}
}
