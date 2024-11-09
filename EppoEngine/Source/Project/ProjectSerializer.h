#pragma once

#include "Project/Project.h"

namespace Eppo
{
	class ProjectSerializer
	{
	public:
		ProjectSerializer(Ref<Project> project);

		bool Serialize();
		bool Deserialize(const std::filesystem::path& filepath);

	private:
		Ref<Project> m_Project;
	};
}
