#include "Panels/ContentBrowserPanel.h"

#include "Renderer/Image.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <ranges>

namespace Eppo
{
	namespace
	{
		constexpr float THUMBNAIL_SIZE = 80.0f;
		constexpr float CELL_PADDING = 20.0f;

		auto ContainsCaseInsensitive(const std::string& haystack, const std::string& needle) -> bool
		{
			if (needle.empty())
				return true;

			const auto it = std::search(
				haystack.begin(), haystack.end(),
				needle.begin(), needle.end(),
				[](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });

			return it != haystack.end();
		}

		// Append " N" before the extension until the path is free, so repeated
		// "New Folder"/"New Scene" actions don't clobber existing entries.
		auto MakeUniquePath(const std::filesystem::path& desired) -> std::filesystem::path
		{
			if (!FS::Exists(desired))
				return desired;

			const auto stem = desired.stem().string();
			const auto ext = desired.extension().string();
			const auto dir = desired.parent_path();

			for (int i = 1; ; ++i)
			{
				auto candidate = dir / (stem + " " + std::to_string(i) + ext);
				if (!FS::Exists(candidate))
					return candidate;
			}
		}

		auto IsImportable(AssetType type) -> bool
		{
			// Only types with an importer registered engine-side are worth
			// adding to the registry from the browser.
			return type == AssetType::Mesh || type == AssetType::Scene;
		}

		auto GetImportDirectory(const std::filesystem::path& assetsDirectory, AssetType type) -> std::filesystem::path
		{
			switch (type)
			{
				case AssetType::Mesh:    return assetsDirectory / "Meshes";
				case AssetType::Scene:   return assetsDirectory / "Scenes";
				case AssetType::Texture: return assetsDirectory / "Textures";
				case AssetType::Script:  return assetsDirectory / "Scripts";
				case AssetType::None:
				default:                 return assetsDirectory;
			}
		}
	}

	auto ContentBrowserPanel::RenderGui() -> void
	{
		ScopedBegin scopedBegin("Content Browser");

		if (!m_IconsLoaded)
			LoadIcons();

		SyncToActiveProject();

		if (m_BaseDirectory.empty())
		{
			ImGui::TextDisabled("No project open.");
			return;
		}

		DrawTopBar();
		ImGui::Separator();

		ImGui::BeginChild("##cb_tree", ImVec2(180.0f, 0.0f), ImGuiChildFlags_Borders);
		DrawFolderTree(m_BaseDirectory);
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##cb_contents");
		DrawContents();
		DrawContextMenus();
		ImGui::EndChild();
	}

	auto ContentBrowserPanel::SyncToActiveProject() -> void
	{
		if (!Project::GetActive())
		{
			m_BaseDirectory.clear();
			m_CurrentDirectory.clear();
			return;
		}

		const auto assetsDirectory = Project::GetAssetsDirectory();
		if (m_BaseDirectory != assetsDirectory)
		{
			m_BaseDirectory = assetsDirectory;
			m_CurrentDirectory = assetsDirectory;
		}

		// The current folder may have been deleted (here or elsewhere); fall back
		// to the root so we never iterate a missing directory.
		if (!FS::Exists(m_CurrentDirectory))
			m_CurrentDirectory = m_BaseDirectory;
	}

	auto ContentBrowserPanel::LoadIcons() -> void
	{
		const auto load = [](const char* fileName) -> Ref<Image>
		{
			const auto path = FS::GetResourcesDirectory() / "Icons" / fileName;
			if (!FS::Exists(path))
			{
				Log::Error("Content browser icon not found: '{}'", path);
				return nullptr;
			}

			ImageSpecification spec;
			spec.ImageFormat = nvrhi::Format::SRGBA8_UNORM;
			spec.DebugName = fileName;

			return CreateRef<Image>(spec, ImageSource(path));
		};

		m_DirectoryIcon = load("Directory.png");
		m_FileIcon = load("File.png");
		m_MeshIcon = load("Mesh.png");
		m_SceneIcon = load("Scene.png");
		m_ScriptIcon = load("Script.png");
		m_TextureIcon = load("Texture.png");
		m_UnknownIcon = load("Unknown.png");

		m_IconsLoaded = true;
	}

	auto ContentBrowserPanel::GetIcon(AssetType type) const -> const Ref<Image>&
	{
		switch (type)
		{
			case AssetType::Mesh:    return m_MeshIcon;
			case AssetType::Scene:   return m_SceneIcon;
			case AssetType::Texture: return m_TextureIcon;
			case AssetType::Script:  return m_ScriptIcon;
			case AssetType::None:    return m_UnknownIcon;
			default:                 return m_FileIcon;
		}
	}

	auto ContentBrowserPanel::DrawTopBar() -> void
	{
		ImGui::BeginDisabled(m_CurrentDirectory == m_BaseDirectory);
		if (ImGui::Button("<-"))
			m_CurrentDirectory = m_CurrentDirectory.parent_path();
		ImGui::EndDisabled();

		// Breadcrumbs from the project directory down, so the "Assets" root is
		// clickable too.
		const auto rootDirectory = m_BaseDirectory.parent_path();
		auto accumulated = rootDirectory;
		for (const auto& part : std::filesystem::relative(m_CurrentDirectory, rootDirectory))
		{
			accumulated /= part;

			ImGui::SameLine();
			if (ImGui::Button(part.string().c_str()))
				m_CurrentDirectory = accumulated;

			ImGui::SameLine();
			ImGui::TextUnformatted("/");
		}

		ImGui::SameLine();
		if (ImGui::Button("Import"))
			ImportFile();

		ImGui::SameLine();
		ImGui::SetNextItemWidth(200.0f);
		ImGui::InputTextWithHint("##cb_search", "Search", m_SearchBuffer, sizeof(m_SearchBuffer));
	}

	auto ContentBrowserPanel::DrawFolderTree(const std::filesystem::path& directory) -> void
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (directory == m_CurrentDirectory)
			flags |= ImGuiTreeNodeFlags_Selected;
		if (directory == m_BaseDirectory)
			flags |= ImGuiTreeNodeFlags_DefaultOpen;

		const std::string label = directory == m_BaseDirectory ? "Assets" : directory.filename().string();

		const bool opened = ImGui::TreeNodeEx(label.c_str(), flags);
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			m_CurrentDirectory = directory;

		if (opened)
		{
			std::error_code ec;
			for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
			{
				if (entry.is_directory())
					DrawFolderTree(entry.path());
			}
			ImGui::TreePop();
		}
	}

	auto ContentBrowserPanel::DrawContents() -> void
	{
		if (ImGui::BeginPopupContextWindow("##cb_window_ctx", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
		{
			const auto& assetManager = Project::GetActive()->GetAssetManager();

			if (ImGui::MenuItem("New Folder"))
				FS::CreateDir(MakeUniquePath(m_CurrentDirectory / "New Folder"));

			if (ImGui::MenuItem("New Scene"))
			{
				const auto path = MakeUniquePath(m_CurrentDirectory / "New Scene.epscene");
				const auto scene = CreateRef<Scene>();
				SceneSerializer(scene).Serialize(path);
				assetManager->CreateAsset(path, scene);
			}

			if (ImGui::MenuItem("Import File..."))
				ImportFile();

			ImGui::EndPopup();
		}

		const auto& assetManager = Project::GetActive()->GetAssetManager();
		const std::string search = m_SearchBuffer;

        constexpr float cellSize = THUMBNAIL_SIZE + CELL_PADDING;
		const float panelWidth = ImGui::GetContentRegionAvail().x;
		const int columnCount = std::max(1, static_cast<int>(panelWidth / cellSize));

		ImGui::Columns(columnCount, nullptr, false);

		std::vector<std::filesystem::directory_entry> entries;
		for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
			entries.push_back(entry);

		// Folders first, then files; alphabetical within each group.
		std::ranges::sort(
            entries, [](const auto& a, const auto& b)
		{
			if (a.is_directory() != b.is_directory())
				return a.is_directory();
			return a.path().filename() < b.path().filename();
		});

		for (const auto& entry : entries)
		{
			const auto& path = entry.path();
			const std::string fileName = path.filename().string();

			if (!ContainsCaseInsensitive(fileName, search))
				continue;

			// The registry only stores real assets; skip our own bookkeeping file.
			if (fileName == "AssetRegistry.json")
				continue;

			const bool isDirectory = entry.is_directory();
			const AssetType type = isDirectory ? AssetType::None : AssetManager::GetAssetTypeFromPath(path);
			const AssetHandle handle = isDirectory ? AssetHandle(0) : assetManager->GetHandleForPath(path);

			ImGui::PushID(fileName.c_str());

			const Ref<Image>& icon = isDirectory ? m_DirectoryIcon : GetIcon(type);

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			if (icon)
				ImGui::ImageButton("##icon", ImGuiEx::CreateTextureRef(icon->GetTexture()), ImVec2(THUMBNAIL_SIZE, THUMBNAIL_SIZE));
			else
				ImGui::Button("##icon", ImVec2(THUMBNAIL_SIZE, THUMBNAIL_SIZE));
			ImGui::PopStyleColor();

			// Registered assets can be dragged onto other panels (e.g. a mesh onto
			// a MeshComponent slot in the Property panel).
			if (handle && ImGui::BeginDragDropSource())
			{
				const uint64_t payload = static_cast<uint64_t>(handle);
				ImGui::SetDragDropPayload("ASSET_HANDLE", &payload, sizeof(payload));
				ImGui::TextUnformatted(fileName.c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				if (isDirectory)
					m_CurrentDirectory = path;
				else
					OpenAsset(path);
			}

			if (ImGui::BeginPopupContextItem())
			{
				if (isDirectory && ImGui::MenuItem("Open"))
					m_CurrentDirectory = path;

				if (!isDirectory && ImGui::MenuItem("Open"))
					OpenAsset(path);

				if (!isDirectory && !handle && IsImportable(type) && ImGui::MenuItem("Import"))
					assetManager->CreateAsset(path);

				if (ImGui::MenuItem("Rename"))
				{
					m_RenameTarget = path;
					m_RenameBuffer = fileName;
					m_OpenRenamePopup = true;
				}

				if (ImGui::MenuItem("Delete"))
				{
					m_DeleteTarget = path;
					m_OpenDeletePopup = true;
				}

				ImGui::EndPopup();
			}

			ImGui::TextWrapped("%s", fileName.c_str());

			ImGui::NextColumn();
			ImGui::PopID();
		}

		ImGui::Columns(1);
	}

	auto ContentBrowserPanel::DrawContextMenus() -> void
	{
		// Trigger the modals here, at the same id-stack level as their
		// BeginPopupModal, so OpenPopup and the modal resolve to the same id.
		if (m_OpenRenamePopup)
		{
			ImGui::OpenPopup("Rename##cb");
			m_OpenRenamePopup = false;
		}

		if (m_OpenDeletePopup)
		{
			ImGui::OpenPopup("Delete?##cb");
			m_OpenDeletePopup = false;
		}

		if (ImGui::BeginPopupModal("Rename##cb", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::InputText("##cb_rename", &m_RenameBuffer);

			if (ImGui::Button("Rename", ImVec2(100.0f, 0.0f)) && !m_RenameBuffer.empty())
			{
                if (const auto newPath = m_RenameTarget.parent_path() / m_RenameBuffer; !FS::Exists(newPath))
				{
					const auto& assetManager = Project::GetActive()->GetAssetManager();

                    if (const AssetHandle handle = assetManager->GetHandleForPath(m_RenameTarget);
                        FS::Move(m_RenameTarget, newPath) && handle)
						assetManager->UpdateAssetPath(handle, newPath);
				}
				else
				{
					Log::Warn("Cannot rename to '{}': a file with that name already exists.", newPath);
				}

				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		if (ImGui::BeginPopupModal("Delete?##cb", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Delete '%s'?", m_DeleteTarget.filename().string().c_str());
			ImGui::TextDisabled("This cannot be undone.");

			if (ImGui::Button("Delete", ImVec2(100.0f, 0.0f)))
			{
				DeletePath(m_DeleteTarget);
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	auto ContentBrowserPanel::OpenAsset(const std::filesystem::path& path) -> void
	{
		const AssetType type = AssetManager::GetAssetTypeFromPath(path);
		if (type != AssetType::Scene)
			return;

		const auto& assetManager = Project::GetActive()->GetAssetManager();

		// Register the scene on first open so it has a stable handle to load by.
		AssetHandle handle = assetManager->GetHandleForPath(path);
		if (!handle)
		{
			assetManager->CreateAsset(path);
			handle = assetManager->GetHandleForPath(path);
		}

		if (m_OpenScene && handle)
			m_OpenScene(handle);
		else if (!m_OpenScene)
			Log::Warn("No open-scene handler wired to the content browser.");
	}

	auto ContentBrowserPanel::ImportFile() -> void
	{
		const auto source = FileDialog::OpenFile({
			{ "Importable Assets", "gltf,glb,epscene,png,jpg,jpeg,tga,bmp,hdr" }
		}, m_CurrentDirectory);

		if (source.empty())
			return;

		const AssetType type = AssetManager::GetAssetTypeFromPath(source);
		const auto destinationDirectory = GetImportDirectory(m_BaseDirectory, type);
		if (!FS::CreateDir(destinationDirectory))
			return;

		auto destination = destinationDirectory / source.filename();
		if (FS::Exists(destination))
			destination = MakeUniquePath(destination);

		std::error_code ec;
		std::filesystem::copy_file(source, destination, ec);
		if (ec)
		{
			Log::Error("Failed to import '{}': {}", source, ec.message());
			return;
		}

		if (IsImportable(type))
			Project::GetActive()->GetAssetManager()->CreateAsset(destination);
	}

	auto ContentBrowserPanel::DeletePath(const std::filesystem::path& path) -> void
	{
		const auto& assetManager = Project::GetActive()->GetAssetManager();
		if (const AssetHandle handle = assetManager->GetHandleForPath(path))
			assetManager->RemoveAsset(handle);

		std::error_code ec;
		if (std::filesystem::is_directory(path))
			std::filesystem::remove_all(path, ec);
		else
			std::filesystem::remove(path, ec);

		if (ec)
			Log::Error("Failed to delete '{}': {}", path, ec.message());
	}
}
