#include "ContentBrowserPanel.h"

namespace Eppo
{
	ContentBrowserPanel::ContentBrowserPanel(PanelManager& panelManager)
		: Panel(panelManager)
	{}

	void ContentBrowserPanel::RenderGui()
	{
		ScopedBegin scopedBegin("Content Browser Panel");

		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

		UpdateFileList();
	}

	void ContentBrowserPanel::UpdateFileList()
	{
		const auto& assetDir = Filesystem::GetAssetsDirectory();

		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
		
		if (ImGui::BeginTable("Files", 4, flags))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn("Loaded", ImGuiTableColumnFlags_WidthFixed, 30.0f);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 60.0f);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 60.0f);
			ImGui::TableHeadersRow();

			TraverseDirectory(assetDir);

			ImGui::EndTable();
		}
	}

	void ContentBrowserPanel::TraverseDirectory(const std::filesystem::path& path, uint32_t depth)
	{
		for (const auto& entry : std::filesystem::directory_iterator(path))
		{
			if (entry.is_directory())
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				bool open = ImGui::TreeNodeEx(entry.path().filename().string().c_str(), ImGuiTreeNodeFlags_OpenOnArrow);
				ImGui::TableNextColumn();

				ImGui::TableNextColumn();
				ImGui::TableNextColumn();
				ImGui::Text("Folder");

				if (open)
				{
					TraverseDirectory(entry, depth + 1);
					ImGui::TreePop();
				}
			}
			else {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::TreeNodeEx(entry.path().filename().string().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
				if (ImGui::BeginDragDropSource())
				{
					auto relativePath = std::filesystem::relative(entry, Filesystem::GetAppRootDirectory());
					const wchar_t* itemPath = relativePath.c_str();

					ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t));
					ImGui::EndDragDropSource();
				}

				ImGui::TableNextColumn();

				// is asset loaded?
				auto relativePath = std::filesystem::relative(entry, Filesystem::GetAppRootDirectory());
				if (AssetManager::Get().IsAssetLoaded(relativePath))
					ImGui::TextUnformatted("Yes");
				else
					ImGui::TextUnformatted("No");

				ImGui::TableNextColumn();
				ImGui::Text("%.2f KB", std::filesystem::file_size(entry.path()) / 1024.0f);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("File");
			}
		}
	}
}
