#pragma once

#include "Panel/Panel.h"

namespace Eppo
{
	class ContentBrowserPanel : public Panel
	{
	public:
		ContentBrowserPanel(PanelManager& panelManager);

		void RenderGui() override;

		void UpdateFileList();

	private:
		void TraverseDirectory(const std::filesystem::path& path, uint32_t depth = 0);

	private:
		struct FileTreeNode
		{
			std::string Name;
			std::string Type;
			size_t Size;
			bool IsFolder = false;
			bool IsOpen = false;
		};

		std::vector<FileTreeNode> m_FileTreeNodes;
	};
}
