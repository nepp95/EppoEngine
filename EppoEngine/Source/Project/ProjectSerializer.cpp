#include "pch.h"
#include "ProjectSerializer.h"

#include "Utility/Json.h"

namespace Eppo
{
    ProjectSerializer::ProjectSerializer(const Ref<Project>& project)
        : m_Project(project)
    {}

    bool ProjectSerializer::Serialize() const
    {
        EPPO_PROFILE_FUNCTION("ProjectSerializer::Serialize");

        const auto& spec = m_Project->GetSpecification();

        nlohmann::json data;

        data = {
            { "Name", spec.Name },
            { "ProjectDirectory", spec.ProjectDirectory.string() },
            { "StartScene", static_cast<uint64_t>(spec.StartScene) }
        };

        Filesystem::WriteText(spec.ProjectDirectory / std::filesystem::path(spec.Name + ".epproj"), data.dump(4));

        return true;
    }

    bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath) const
    {
        EPPO_PROFILE_FUNCTION("ProjectSerializer::Deserialize");

        auto& spec = m_Project->GetSpecification();

        std::ifstream stream(filepath);
        nlohmann::json data;

        try
        {
            data = nlohmann::json::parse(stream);
        }
        catch (nlohmann::json::exception& e)
        {
            EPPO_ERROR("Failed to load project file '{}'!", filepath);
            EPPO_ERROR("Parse Error: {}", e.what());
            return false;
        }

        spec.Name = data["Name"].get<std::string>();
        EPPO_INFO("Deserializing project '{}'", spec.Name);

        if (data.contains("ProjectDirectory"))
            spec.ProjectDirectory = std::filesystem::path(data["ProjectDirectory"].get<std::string>());

        if (data.contains("StartScene"))
            spec.StartScene = data["StartScene"].get<UUID>();

        return true;
    }
}
