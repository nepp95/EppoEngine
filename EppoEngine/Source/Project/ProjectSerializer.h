#pragma once

#include "Project/Project.h"

namespace Eppo
{
    class ProjectSerializer
    {
    public:
        explicit ProjectSerializer(const Ref<Project>& project);

        [[nodiscard]] auto Serialize() const -> bool;
        [[nodiscard]] auto Deserialize(const std::filesystem::path& path) const -> bool;

    private:
        Ref<Project> m_Project = nullptr;
    };
}