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

SUITE(Scenario)
{
    constexpr float DEFAULT_TOLERANCE = 1e-4f;

    TEST(EditorCamera_HoldForwardKey_MovesForward)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        EditorCamera camera(glm::vec3(0.0f), 0.0f, 0.0f);
        const glm::vec3 start = camera.GetPosition();

        ctx.GetInput().PressKey(Key::W);
        ctx.AdvanceFrames(30, [&](const float ts) { camera.OnUpdate(ts); });
        const glm::vec3 moved = camera.GetPosition() - start;

        CHECK_CLOSE(1.5f, moved.x, 0.1f);
        CHECK(moved.x > 1.0f);
        CHECK_CLOSE(0.0f, moved.y, DEFAULT_TOLERANCE);
        CHECK_CLOSE(0.0f, moved.z, DEFAULT_TOLERANCE);
    }

    TEST(EditorCamera_NoInput_StaysPut)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        EditorCamera camera(glm::vec3(0.0f), 0.0f, 0.0f);
        ctx.AdvanceFrames(30, [&](const float ts) { camera.OnUpdate(ts); });
        CHECK_VEC3_CLOSE(glm::vec3(0.0f), camera.GetPosition(), DEFAULT_TOLERANCE);
    }

    TEST(EditorCamera_ShiftModifier_BoostsSpeed)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        EditorCamera camera(glm::vec3(0.0f), 0.0f, 0.0f);

        ctx.GetInput().PressKey(Key::W);
        ctx.GetInput().PressKey(Key::LeftShift);
        ctx.AdvanceFrames(30, [&](const float ts) { camera.OnUpdate(ts); });

        CHECK_CLOSE(4.5f, camera.GetPosition().x, 0.3f);
        CHECK(camera.GetPosition().x > 3.0f);
    }

    TEST(EditorCamera_ReleaseKey_StopsMovement)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        EditorCamera camera(glm::vec3(0.0f), 0.0f, 0.0f);

        ctx.GetInput().PressKey(Key::W);
        ctx.AdvanceFrames(10, [&](const float ts) { camera.OnUpdate(ts); });
        const float afterHold = camera.GetPosition().x;

        ctx.GetInput().ReleaseKey(Key::W);
        ctx.AdvanceFrames(10, [&](const float ts) { camera.OnUpdate(ts); });

        CHECK_CLOSE(afterHold, camera.GetPosition().x, DEFAULT_TOLERANCE);
        CHECK(afterHold > 0.0f);
    }

    TEST(Scene_SpawnedEntity_PersistsAcrossFrames)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        Entity player = ctx.GetScene()->CreateEntity("Player");
        player.GetComponent<TransformComponent>().Translation = { 5.0f, 0.0f, -3.0f };

        ctx.AdvanceFrames(20);

        CHECK(static_cast<bool>(player));
        CHECK_EQUAL(std::string("Player"), player.GetName());
        CHECK_VEC3_CLOSE(glm::vec3(5.0f, 0.0f, -3.0f), player.GetComponent<TransformComponent>().Translation, DEFAULT_TOLERANCE);
    }

    TEST(SceneSerializer_HarnessScene_LoadsThreeEntities)
    {
        Testing::TestContext ctx;
        if (!ctx.IsAvailable())
            return;

        // Load the committed testing scene; Scene has no enumeration, so read back via serialization.
        const auto scenePath = FS::GetRootDirectory() / "TestData" / "Scenes" / "harness.epscene";
        REQUIRE CHECK(FS::Exists(scenePath));
        REQUIRE CHECK(SceneSerializer(ctx.GetScene()).Deserialize(scenePath));

        const Testing::TempDir dir;
        const auto out = dir.File("readback.epscene");
        bool result = SceneSerializer(ctx.GetScene()).Serialize(out);
        REQUIRE CHECK(result);

        std::ifstream stream(out);
        const json data = json::parse(stream);
        const auto& entities = data["Scene"]["Entities"];

        CHECK_EQUAL(3u, entities.size());
        // Entities are ID-sorted (1001 Ground, 1002 Player, 1003 Prop).
        CHECK_EQUAL(std::string("Ground"), entities[0]["TagComponent"]["Tag"].get<std::string>());
        CHECK_EQUAL(std::string("Player"), entities[1]["TagComponent"]["Tag"].get<std::string>());
        CHECK_EQUAL(1003ull, entities[2]["IDComponent"]["ID"].get<uint64_t>());
    }

    TEST(SceneRenderer_PointLightAndGradientSky_RendersWithoutError)
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

        const Ref<SceneRenderer> sceneRenderer = CreateRef<SceneRenderer>(scene, SceneRendererSpecification{ .Width = 256u, .Height = 256u });
        const EditorCamera camera(glm::vec3(0.0f, 2.0f, 6.0f), 0.0f, 0.0f);

        // Render across several real frames to cycle the frames-in-flight indices.
        ctx.AdvanceFrames(3, [&](float) { scene->OnRenderEditor(sceneRenderer, camera); });

        CHECK(sceneRenderer->GetFinalImage() != nullptr);
        CHECK(Testing::AppHarness::Get()->IsRunning());
    }
}
