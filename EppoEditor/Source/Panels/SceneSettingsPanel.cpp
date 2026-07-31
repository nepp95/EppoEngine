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

        const auto project = Project::GetActive();
        const auto assetManager = project ? project->GetAssetManager() : nullptr;

        EnvironmentSettings& environment = scene->GetEnvironmentSettings();
        ImGui::SeparatorText("Environment");
        ImGui::PushID("Environment");
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

        ImGui::DragFloat("Intensity", &environment.AmbientIntensity, 0.01f, 0.0f, 100.0f);

        ImGui::ColorEdit3("Zenith", glm::value_ptr(environment.ZenithColor));
        ImGui::ColorEdit3("Horizon", glm::value_ptr(environment.HorizonColor));
        ImGui::ColorEdit3("Ground", glm::value_ptr(environment.GroundColor));
        ImGui::PopID();

        auto& bloom = scene->GetBloomSettings();
        ImGui::SeparatorText("Bloom");
        ImGui::PushID("Bloom");
        ImGui::DragFloat("Threshold", &bloom.Threshold, 0.01f, 0.0f);
        ImGui::DragFloat("Knee", &bloom.Knee, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Intensity", &bloom.Intensity, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Radius", &bloom.Radius, 0.01f, 0.0f, 4.0f);
        ImGui::PopID();
    }
}
