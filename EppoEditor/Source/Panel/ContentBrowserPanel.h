#pragma once

#include "Panel/Panel.h"

namespace Eppo
{
	class ContentBrowserPanel : public Panel
	{
	public:
		ContentBrowserPanel(PanelManager& panelManager);

		void RenderGui() override;

	private:
		void TraverseDirectory(const std::filesystem::path& path);

	private:
		struct FileTreeNode
		{
			std::string Name;
			std::string Type;
			size_t Size;
			bool Loaded = false;
			bool IsFolder = false;
		};

		std::vector<FileTreeNode> m_FileTreeNodes;
	};
}
