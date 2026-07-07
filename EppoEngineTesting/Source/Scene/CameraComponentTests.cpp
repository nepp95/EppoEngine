#include "pch.h" // Engine headers below rely on the precompiled header for Ref<>, EP_ASSERT, std includes, etc.

#include <UnitTest++/UnitTest++.h>

#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

using namespace Eppo;

namespace
{
	SUITE(CameraComponent)
	{
		TEST(SceneCameraViewportChangesProjection)
		{
			SceneCamera camera;
			camera.SetPerspective(45.0f, 0.1f, 100.0f);

			camera.SetViewportSize(1600, 900);
			const glm::mat4 wide = camera.GetProjectionMatrix();

			camera.SetViewportSize(800, 800);
			const glm::mat4 square = camera.GetProjectionMatrix();

			// Aspect ratio feeds the horizontal scale (element [0][0]); changing it
			// must change the projection.
			CHECK(wide[0][0] != square[0][0]);
		}

		TEST(SceneCameraZeroViewportKeepsProjection)
		{
			SceneCamera camera;
			camera.SetViewportSize(1280, 720);
			const glm::mat4 before = camera.GetProjectionMatrix();

			camera.SetViewportSize(0, 0);
			const glm::mat4 after = camera.GetProjectionMatrix();

			CHECK_CLOSE(before[0][0], after[0][0], 1e-6f);
			CHECK_CLOSE(before[1][1], after[1][1], 1e-6f);
		}

		TEST(NoCameraEntityReturnsInvalidEntity)
		{
			Scene scene;
			scene.CreateEntity("Just a mesh holder");

			CHECK(!scene.GetPrimaryCameraEntity());
		}

		TEST(PrimaryCameraFirstPrimaryWins)
		{
			Scene scene;

			Entity nonPrimary = scene.CreateEntity("Secondary");
			nonPrimary.AddComponent<CameraComponent>().Primary = false;

			Entity primary = scene.CreateEntity("Main");
			primary.AddComponent<CameraComponent>().Primary = true;

			const Entity found = scene.GetPrimaryCameraEntity();
			CHECK(found);
			CHECK_EQUAL(primary.GetUUID(), found.GetUUID());
		}

		TEST(SceneCopyPreservesCameraComponent)
		{
			Ref<Scene> scene = CreateRef<Scene>();

			Entity camera = scene->CreateEntity("Camera");
			auto& cc = camera.AddComponent<CameraComponent>();
			cc.Primary = true;
			cc.Camera.SetPerspective(60.0f, 0.2f, 250.0f);

			Ref<Scene> copy = Scene::Copy(scene);

			const Entity copied = copy->GetPrimaryCameraEntity();
			CHECK(copied);

			const auto& copiedCamera = copied.GetComponent<CameraComponent>().Camera;
			CHECK_CLOSE(60.0f, copiedCamera.GetPerspectiveVerticalFov(), 1e-4f);
			CHECK_CLOSE(0.2f, copiedCamera.GetPerspectiveNearClip(), 1e-4f);
			CHECK_CLOSE(250.0f, copiedCamera.GetPerspectiveFarClip(), 1e-4f);
		}
	}
}
