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

		ImGui::BeginGroup();

		const auto assetManager = Project::GetActive()->GetAssetManagerEditor();
		const auto& assetRegistry = assetManager->GetAssetRegistry();
		const AssetType currentAssetType = Utils::AssetTypeFromString(items[currentItem]);

		for (const auto& [handle, metadata] : assetRegistry)
		{
			if (currentAssetType != metadata.Type)
				continue;

			Ref<Image> thumbnail = m_ThumbnailCache.GetOrCreateThumbnail(currentAssetType);

			ImGui::BeginGroup();

			UI::ImageButton(metadata.GetName(), thumbnail, ImVec2(128.0f, 128.0f), ImVec2(0, 1), ImVec2(1, 0));
			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload(Utils::AssetTypeToImGuiPayloadType(currentAssetType), &handle, sizeof(AssetHandle));
				ImGui::EndDragDropSource();
			}

			ImGui::TextDisabled(metadata.GetName().c_str());
			ImGui::EndGroup();
			ImGui::SameLine();
		}

		ImGui::EndGroup();
	}
}
