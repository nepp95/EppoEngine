#include "Panels/SceneSettingsPanel.h"

#include "ImGui/ScopedBegin.h"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

namespace Eppo
{
    auto SceneSettingsPanel::RenderGui() -> void
    {
        ScopedBegin scopedBegin("Scene Settings");

        const Ref<Scene>& scene = GetSceneContext();
        if (!scene)
            return;

        EnvironmentSettings& environment = scene->GetEnvironment();
        const auto project = Project::GetActive();
        const auto assetManager = project ? project->GetAssetManager() : nullptr;

        ImGui::SeparatorText("Skybox");
        if (environment.SkyboxHandle)
        {
            if (assetManager && assetManager->HasAssetData(environment.SkyboxHandle))
            {
                const auto& metadata = assetManager->GetMetadata(environment.SkyboxHandle);
                ImGui::TextDisabled("%s", metadata.Filepath.filename().string().c_str());
            }
            else
            {
                ImGui::TextDisabled("<Missing texture>");
            }

            ImGui::SameLine();
            if (ImGui::Button("X"))
                environment.SkyboxHandle = 0;
        }
        else
        {
            ImGui::Button("Skybox", ImVec2(100.0f, 0.0f));
            if (ImGui::BeginDragDropTarget())
            {
                // Assets dragged from the content browser carry their handle.
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_HANDLE"))
                {
                    const AssetHandle handle = *static_cast<const uint64_t*>(payload->Data);
                    if (assetManager && assetManager->HasAssetData(handle) && assetManager->GetMetadata(handle).Type == AssetType::Texture)
                        environment.SkyboxHandle = handle;
                    else
                        Log::Warn("Dropped asset is not a texture; ignoring.");
                }
                ImGui::EndDragDropTarget();
            }
        }

        ImGui::SeparatorText("Ambient");
        ImGui::DragFloat("Intensity", &environment.AmbientIntensity, 0.01f, 0.0f, 100.0f);

        ImGui::SeparatorText("Gradient sky (fallback)");
        ImGui::ColorEdit3("Zenith", glm::value_ptr(environment.ZenithColor));
        ImGui::ColorEdit3("Horizon", glm::value_ptr(environment.HorizonColor));
        ImGui::ColorEdit3("Ground", glm::value_ptr(environment.GroundColor));
    }
}
