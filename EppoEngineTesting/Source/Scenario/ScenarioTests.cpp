// pch first: this TU pulls in heavy engine headers (SceneRenderer -> Application,
// nvrhi, ...) and test TUs do not get the engine PCH automatically.
#include "pch.h"

#include "Support/AppHarness.h"
#include "Support/EppoTest.h"
#include "Support/GlmCheck.h"
#include "Support/TempDir.h"
#include "Support/TestContext.h"

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Scene/SceneSerializer.h"

#include <nlohmann/json.hpp>

using namespace Eppo;
using json = nlohmann::json;

// Scenario suite (label `graphical`): Factorio-style automated scenarios that
// run the real, on-screen application and drive it — simulate input, advance
// real frames, mutate the scene — then assert on the resulting state. These are
// the capstone that ties every seam together: the live app (StepFrame), input
// simulation (SimulatedInput behind the Input facade), and the ECS/serializer.
//
// Requires a display + GPU; excluded on headless CI (`ctest -LE graphical`).
SUITE(Scenario)
{
    constexpr float kTol = 1e-4f;

    // With pitch=0, yaw=0 the camera's front axis is +X and its right axis is +Z
    // (see EditorCamera::UpdateCameraVectors), so movement directions are known.
    // Per frame the camera moves MovementSpeed(3) * timestep(1/60) = 0.05 units.

    TEST(CameraMovesForwardWhileKeyHeld)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        EditorCamera camera(glm::vec3(0.0f), 0.0f, 0.0f);
        const glm::vec3 start = camera.GetPosition();

        // Hold W and let the live app run 30 frames, driving the real camera
        // update each frame through the simulated input.
        ctx.GetInput().PressKey(Key::W);
        ctx.AdvanceFrames(30, [&](float ts) { camera.OnUpdate(ts); });

        const glm::vec3 moved = camera.GetPosition() - start;
        // ~30 frames * 0.05 = 1.5 units along +X, none sideways/vertical. The
        // forward magnitude uses a looser tolerance (~2 frames) since OnUpdate is
        // driven by StepFrame and only fires on frames the device actually begins;
        // the perpendicular axes are exact regardless of how many frames ran.
        CHECK_CLOSE(1.5f, moved.x, 0.1f);
        CHECK(moved.x > 1.0f);
        CHECK_CLOSE(0.0f, moved.y, kTol);
        CHECK_CLOSE(0.0f, moved.z, kTol);
    }

    TEST(CameraStaysPutWithoutInput)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        EditorCamera camera(glm::vec3(0.0f), 0.0f, 0.0f);

        // No keys pressed: stepping frames must not move the camera.
        ctx.AdvanceFrames(30, [&](float ts) { camera.OnUpdate(ts); });

        CHECK_VEC3_CLOSE(glm::vec3(0.0f), camera.GetPosition(), kTol);
    }

    TEST(ShiftBoostsMovementSpeed)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        EditorCamera camera(glm::vec3(0.0f), 0.0f, 0.0f);

        // Shift triples the velocity: ~30 * 0.05 * 3 = 4.5 units along +X (loose
        // tolerance for the frame-count sensitivity noted above).
        ctx.GetInput().PressKey(Key::W);
        ctx.GetInput().PressKey(Key::LeftShift);
        ctx.AdvanceFrames(30, [&](float ts) { camera.OnUpdate(ts); });

        CHECK_CLOSE(4.5f, camera.GetPosition().x, 0.3f);
        CHECK(camera.GetPosition().x > 3.0f);
    }

    TEST(ReleasingKeyStopsMovement)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        EditorCamera camera(glm::vec3(0.0f), 0.0f, 0.0f);

        ctx.GetInput().PressKey(Key::W);
        ctx.AdvanceFrames(10, [&](float ts) { camera.OnUpdate(ts); });
        const float afterHold = camera.GetPosition().x;

        // Release: subsequent frames must not advance the camera further.
        ctx.GetInput().ReleaseKey(Key::W);
        ctx.AdvanceFrames(10, [&](float ts) { camera.OnUpdate(ts); });

        CHECK_CLOSE(afterHold, camera.GetPosition().x, kTol);
        CHECK(afterHold > 0.0f);
    }

    TEST(SpawnedEntityPersistsAcrossFrames)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        // Create an entity in the live scene, then run real frames. Nothing in
        // this scenario mutates it, so its state must survive the frames intact.
        Entity player = ctx.GetScene()->CreateEntity("Player");
        player.GetComponent<TransformComponent>().Translation = { 5.0f, 0.0f, -3.0f };

        ctx.AdvanceFrames(20);

        CHECK(static_cast<bool>(player));
        CHECK_EQUAL(std::string("Player"), player.GetName());
        CHECK_VEC3_CLOSE(glm::vec3(5.0f, 0.0f, -3.0f), player.GetComponent<TransformComponent>().Translation, kTol);
    }

    TEST(LoadsTestingProjectScene)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        // Load the committed testing scene (copied next to the exe) into the
        // live scene, then verify its contents. Scene exposes no enumeration, so
        // we read back through serialization (as in the Scene suite).
        const auto scenePath = FS::GetRootDirectory() / "TestData" / "Scenes" / "harness.epscene";
        REQUIRE CHECK(FS::Exists(scenePath));
        REQUIRE CHECK(SceneSerializer(ctx.GetScene()).Deserialize(scenePath));

        const Testing::TempDir dir;
        const auto out = dir.File("readback.epscene");
        SceneSerializer(ctx.GetScene()).Serialize(out);

        std::ifstream stream(out);
        const json data = json::parse(stream);
        const auto& entities = data["Scene"]["Entities"];

        CHECK_EQUAL(3u, entities.size());
        // Entities are ID-sorted (1001 Ground, 1002 Player, 1003 Prop).
        CHECK_EQUAL(std::string("Ground"), entities[0]["TagComponent"]["Tag"].get<std::string>());
        CHECK_EQUAL(std::string("Player"), entities[1]["TagComponent"]["Tag"].get<std::string>());
        CHECK_EQUAL(1003ull, entities[2]["IDComponent"]["ID"].get<uint64_t>());
    }

    // Physics runs through the live runtime loop: OnRuntimeStart builds the world
    // from the components, each stepped frame advances it and writes the simulated
    // pose back into TransformComponent. A dynamic body must fall under gravity.
    TEST(DynamicBodyFallsInPlayMode)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        const Ref<Scene> scene = ctx.GetScene();

        Entity box = scene->CreateEntity("FallingBox");
        box.GetComponent<TransformComponent>().Translation = { 0.0f, 10.0f, 0.0f };
        box.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;
        box.AddComponent<BoxColliderComponent>();

        const float startY = box.GetComponent<TransformComponent>().Translation.y;

        scene->OnRuntimeStart();
        ctx.AdvanceFrames(60, [&](float ts) { scene->OnUpdateRuntime(ts); });
        scene->OnRuntimeStop();

        CHECK(box.GetComponent<TransformComponent>().Translation.y < startY - 0.1f);
    }

    // A dynamic body with no collider components must still fall: the Scene
    // injects a default 0.5-unit box shape so gravity has mass to act on.
    TEST(DynamicBodyFallsWithoutCollider)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        const Ref<Scene> scene = ctx.GetScene();

        Entity box = scene->CreateEntity("ColliderlessBox");
        box.GetComponent<TransformComponent>().Translation = { 0.0f, 10.0f, 0.0f };
        box.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;
        // Deliberately no collider component — Scene should inject a default.

        const float startY = box.GetComponent<TransformComponent>().Translation.y;

        scene->OnRuntimeStart();
        ctx.AdvanceFrames(60, [&](float ts) { scene->OnUpdateRuntime(ts); });
        scene->OnRuntimeStop();

        CHECK(box.GetComponent<TransformComponent>().Translation.y < startY - 0.1f);
    }

    // Smoke test for the point-light + gradient-sky rendering path. Constructing
    // the SceneRenderer builds both the geometry pipeline and the sky pipeline
    // (whose fullscreen triangle has a zero-attribute input layout), and each
    // rendered frame runs GeometryPass + SkyPass, uploading the light/environment
    // buffers and issuing the background draw. No mesh is used: SubmitMesh needs an
    // active Project/AssetManager, and the pipelines/sky pass are independent of
    // scene geometry. Surviving the frames without a device error is the check.
    TEST(RendersPointLitSceneWithGradientSky)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        const Ref<Scene> scene = ctx.GetScene();

        Entity lamp = scene->CreateEntity("Lamp");
        lamp.GetComponent<TransformComponent>().Translation = { 2.0f, 3.0f, 2.0f };
        auto& light = lamp.AddComponent<PointLightComponent>();
        light.Color = { 1.0f, 0.8f, 0.6f };
        light.Intensity = 15.0f;

        scene->GetEnvironment().AmbientIntensity = 0.75f;

        const Ref<SceneRenderer> sceneRenderer = CreateRef<SceneRenderer>(scene, 256u, 256u);
        const ScopedPtr<EditorCamera> camera = CreateScopedPtr<EditorCamera>(glm::vec3(0.0f, 2.0f, 6.0f), 0.0f, 0.0f);

        // Render across several real frames to cycle the frames-in-flight indices.
        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });

        CHECK(sceneRenderer->GetFinalImage() != nullptr);
        CHECK(Testing::AppHarness::Get()->IsRunning());
    }
}
