#include "Support/EppoTest.h"

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Camera/SceneCamera.h"

using namespace Eppo;

SUITE(Core)
{
    TEST(EditorCamera_UsesZeroToOneDepth)
    {
        EditorCamera editorCamera;
        editorCamera.SetViewportSize(1600u, 900u);
        const glm::vec4 nearPosition = editorCamera.GetProjectionMatrix() * glm::vec4(0.0f, 0.0f, -editorCamera.GetNearClip(), 1.0f);
        const glm::vec4 farPosition = editorCamera.GetProjectionMatrix() * glm::vec4(0.0f, 0.0f, -editorCamera.GetFarClip(), 1.0f);
        CHECK_CLOSE(0.0f, nearPosition.z / nearPosition.w, 0.0001f);
        CHECK_CLOSE(1.0f, farPosition.z / farPosition.w, 0.0001f);
    }

    TEST(SceneCamera_UsesZeroToOneDepth)
    {
        SceneCamera sceneCamera;
        sceneCamera.SetPerspective(glm::radians(45.0f), 0.1f, 100.0f);
        sceneCamera.SetViewportSize(1600u, 900u);
        const glm::vec4 nearPosition =
            sceneCamera.GetProjectionMatrix() * glm::vec4(0.0f, 0.0f, -sceneCamera.GetPerspectiveNearClip(), 1.0f);
        const glm::vec4 farPosition = sceneCamera.GetProjectionMatrix() * glm::vec4(0.0f, 0.0f, -sceneCamera.GetPerspectiveFarClip(), 1.0f);
        CHECK_CLOSE(0.0f, nearPosition.z / nearPosition.w, 0.0001f);
        CHECK_CLOSE(1.0f, farPosition.z / farPosition.w, 0.0001f);
    }
}
