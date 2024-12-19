#pragma once

#include "Panel/Panel.h"
#include "ThumbnailCache.h"

namespace Eppo
{
	class ContentBrowserPanel : public Panel
	{
	public:
		explicit ContentBrowserPanel(PanelManager& panelManager);

		void RenderGui() override;

	private:
		ThumbnailCache m_ThumbnailCache;

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
