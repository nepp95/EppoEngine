#include "ContentBrowserPanel.h"

namespace Eppo
{
	ContentBrowserPanel::ContentBrowserPanel(PanelManager& panelManager)
		: Panel(panelManager)
	{}

	void ContentBrowserPanel::RenderGui()
	{
		ScopedBegin scopedBegin("Content Browser Panel");

		const auto& assetDir = Filesystem::GetAssetsDirectory();
		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

		if (ImGui::BeginTable("Files", 4, flags))
		{
			ImGui::TableSetupColumn("Loaded", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);

			m_FileTreeNodes.clear();
			TraverseDirectory(assetDir);

			for (const auto& node : m_FileTreeNodes)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				if (node.IsFolder)
				{
					bool open = ImGui::TreeNodeEx(node.Name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
					ImGui::TableNextColumn();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Folder");
				}
				else
				{
					ImGui::TreeNodeEx(node.Name.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
					ImGui::TableNextColumn();
					ImGui::Text("%d", node.Size);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(node.Type.c_str());
				}
			}

			ImGui::EndTable();
		}
	}

	void ContentBrowserPanel::TraverseDirectory(const std::filesystem::path& path)
	{
		for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(path))
		{
			std::filesystem::path relativePath = dirEntry.path().filename();

			FileTreeNode& node = m_FileTreeNodes.emplace_back();
			node.Name = relativePath.string();

			if (!dirEntry.is_directory())
			{
				node.Type = relativePath.extension().string();
				node.Size = std::filesystem::file_size(dirEntry.path());
			}
			else
				node.IsFolder = true;
		}
	}
}
