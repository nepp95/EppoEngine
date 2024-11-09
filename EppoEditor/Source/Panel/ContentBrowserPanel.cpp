#include "ContentBrowserPanel.h"

namespace Eppo
{
	namespace Utils
	{
		static const char* GetImGuiPayloadTypeFromExtension(const std::filesystem::path& filepath)
		{
			if (filepath == ".glb") return "MESH_ASSET";
			if (filepath == ".gltf") return "MESH_ASSET";
			if (filepath == ".png") return "TEXTURE_ASSET";
			if (filepath == ".cs")  return "SCRIPT_ASSET";

			return "CONTENT_BROWSER_ITEM";
		}
	}

	ContentBrowserPanel::ContentBrowserPanel(PanelManager& panelManager)
		: Panel(panelManager)
	{
		
	}

	void ContentBrowserPanel::RenderGui()
	{
		ScopedBegin scopedBegin("Content Browser Panel");

		ImGui::BeginGroup();

		constexpr char* items[] = { "Meshes", "Scenes", "Scripts", "Textures" };
		static int currentItem = 0;
		if (ImGui::BeginListBox("##Assets", ImVec2(100.0f, -FLT_MIN)))
		{
			for (int n = 0; n < IM_ARRAYSIZE(items); n++)
			{
				const bool isSelected = (currentItem == n);
				if (ImGui::Selectable(items[n], isSelected))
					currentItem = n;

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndListBox();
		}

		ImGui::EndGroup();
		ImGui::SameLine();

		Ref<AssetManagerEditor> assetManager = Project::GetActive()->GetAssetManagerEditor();
		const auto& assetRegistry = assetManager->GetAssetRegistry();

		const AssetType currentAssetType = Utils::AssetTypeFromString(items[currentItem]);

		for (const auto& [handle, metadata] : assetRegistry)
		{
			if (currentAssetType != metadata.Type)
				continue;

			Ref<Image> thumbnail = m_ThumbnailCache.GetOrCreateThumbnail(currentAssetType);

			ImGui::BeginGroup();

			UI::ImageButton(thumbnail, ImVec2(128.0f, 128.0f), ImVec2(0, 1), ImVec2(1, 0));
			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload(Utils::AssetTypeToImGuiPayloadType(currentAssetType), &handle, sizeof(AssetHandle));
				ImGui::EndDragDropSource();
			}

			ImGui::TextDisabled(metadata.GetName().c_str());

			ImGui::EndGroup();
		}
	}

	void ContentBrowserPanel::UpdateFileList()
	{
		const auto& assetDir = Filesystem::GetAssetsDirectory();

		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
		
		if (ImGui::BeginTable("Files", 4, flags))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
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

					ImGui::SetDragDropPayload(Utils::GetImGuiPayloadTypeFromExtension(relativePath.extension()), itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t));
					ImGui::EndDragDropSource();
				}

				ImGui::TableNextColumn();
				ImGui::Text("%.2f KB", std::filesystem::file_size(entry.path()) / 1024.0f);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("File");
			}
		}
	}
}
