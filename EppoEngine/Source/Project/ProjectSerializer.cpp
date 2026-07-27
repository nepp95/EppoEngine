#include "pch.h"
#include "Project/ProjectSerializer.h"

#include "Utility/Json.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Eppo
{
    ProjectSerializer::ProjectSerializer(const Ref<Project>& project)
        : m_Project(project)
    {}

    auto ProjectSerializer::Serialize() const -> bool
    {
        EP_PROFILE_FN("ProjectSerializer::Serialize");

        const auto& spec = m_Project->GetSpecification();

        Log::Info("Serializing project '{}'", spec.Name);

        json data;
        data["Project"]["Name"] = spec.Name;
        data["Project"]["ProjectDirectory"] = spec.ProjectDirectory.string();
        data["Project"]["StartScene"] = spec.StartScene;

        FS::WriteText(spec.ProjectDirectory / std::filesystem::path(spec.Name + ".epproj"), data.dump(4), true);

        return true;
    }

    auto ProjectSerializer::Deserialize(const std::filesystem::path& path) const -> bool
    {
        EP_PROFILE_FN("ProjectSerializer::Deserialize");

        auto& spec = m_Project->GetSpecification();

        std::ifstream stream(path);
        json data;

        try
        {
            data = json::parse(stream);
        }
        catch (const json::exception& ex)
        {
            Log::Error("Failed to parse project file '{}'!", path);
            Log::Error("Parse error: {}", ex.what());
            return false;
        }

        auto& projectNode = data["Project"];
        if (projectNode.is_null())
        {
            Log::Error("Failed to parse project file. No project node found!");
            return false;
        }

        spec.Name = projectNode["Name"];
        Log::Info("Deserializing project '{}'", spec.Name);

        if (!projectNode["ProjectDirectory"].is_null())
            spec.ProjectDirectory = std::filesystem::path(projectNode["ProjectDirectory"].get<std::string>());

        if (!projectNode["StartScene"].is_null())
            spec.StartScene = projectNode["StartScene"].get<AssetHandle>();

        return true;
    }
}