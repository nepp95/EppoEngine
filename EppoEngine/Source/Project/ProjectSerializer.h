#pragma once

#include "Project/Project.h"

namespace Eppo
{
	class ProjectSerializer
	{
	public:
		explicit ProjectSerializer(const Ref<Project>& project);

		[[nodiscard]] bool Serialize() const;
		[[nodiscard]] bool Deserialize(const std::filesystem::path& filepath) const;

	private:
		Ref<Project> m_Project;
	};
}
