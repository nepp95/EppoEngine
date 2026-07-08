#include "Support/EppoTest.h"
#include "Support/GlmCheck.h"
#include "Support/TempDir.h"

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Scene/SceneSerializer.h"

#include <nlohmann/json.hpp>

using namespace Eppo;
using Testing::TempDir;
using json = nlohmann::json;

// SceneSerializer round-trips scenes to JSON. Scene has no public entity
// enumeration, so we use the serializer itself as the read-back mechanism:
// inspect the emitted JSON directly, and verify Deserialize by re-serializing
// the loaded scene and comparing the (ID-sorted, deterministic) Entities arrays.
SUITE(Scene)
{
    namespace
    {
        // A scene with three entities at known UUIDs: one plain, one with a
        // transform, one with a transform + mesh handle.
        auto BuildCanonicalScene() -> Ref<Scene>
        {
            Ref<Scene> scene = CreateRef<Scene>();

            scene->CreateEntityWithUUID(UUID(100ull), "Alpha");

            Entity beta = scene->CreateEntityWithUUID(UUID(200ull), "Beta");
            auto& betaTransform = beta.GetComponent<TransformComponent>();
            betaTransform.Translation = { 1.0f, 2.0f, 3.0f };
            // Non-default Rotation so the round-trip can actually catch a
            // dropped/corrupted Rotation field (a uniformly-default field is
            // invisible to a serialize->deserialize->reserialize comparison).
            betaTransform.Rotation = { 0.25f, -0.5f, 1.0f };

            Entity gamma = scene->CreateEntityWithUUID(UUID(300ull), "Gamma");
            gamma.GetComponent<TransformComponent>().Scale = { 2.0f, 2.0f, 2.0f };
            gamma.AddComponent<MeshComponent>(AssetHandle(9001ull));

            return scene;
        }

        auto ParseSceneFile(const std::filesystem::path& path) -> json
        {
            std::ifstream stream(path);
            return json::parse(stream);
        }
    }

    TEST(SerializeWritesFile)
    {
        const TempDir dir;
        const auto path = dir.File("world.epscene");

        CHECK(SceneSerializer(BuildCanonicalScene()).Serialize(path));
        CHECK(FS::Exists(path));
    }

    TEST(SerializeEmitsEntitiesWithComponents)
    {
        const TempDir dir;
        const auto path = dir.File("world.epscene");
        SceneSerializer(BuildCanonicalScene()).Serialize(path);

        const json data = ParseSceneFile(path);
        const auto& entities = data["Scene"]["Entities"];

        CHECK_EQUAL(3u, entities.size());

        // Entities are sorted by ascending IDComponent.ID.
        CHECK_EQUAL(100ull, entities[0]["IDComponent"]["ID"].get<uint64_t>());
        CHECK_EQUAL(std::string("Alpha"), entities[0]["TagComponent"]["Tag"].get<std::string>());

        // Beta's translation and (non-default) rotation survive.
        CHECK_EQUAL(200ull, entities[1]["IDComponent"]["ID"].get<uint64_t>());
        const auto translation = entities[1]["TransformComponent"]["Translation"].get<std::vector<float>>();
        CHECK_EQUAL(3u, translation.size());
        CHECK_CLOSE(1.0f, translation[0], 1e-6f);
        CHECK_CLOSE(3.0f, translation[2], 1e-6f);

        const auto rotation = entities[1]["TransformComponent"]["Rotation"].get<std::vector<float>>();
        CHECK_EQUAL(3u, rotation.size());
        CHECK_CLOSE(0.25f, rotation[0], 1e-6f);
        CHECK_CLOSE(-0.5f, rotation[1], 1e-6f);
        CHECK_CLOSE(1.0f, rotation[2], 1e-6f);

        // Only Gamma carries a mesh handle.
        CHECK(!entities[0].contains("MeshComponent"));
        CHECK_EQUAL(9001ull, entities[2]["MeshComponent"]["MeshHandle"].get<uint64_t>());
    }

    TEST(SerializeSortsEntitiesById)
    {
        const TempDir dir;

        // Insert out of order; serialization must still emit ascending IDs.
        Ref<Scene> scene = CreateRef<Scene>();
        scene->CreateEntityWithUUID(UUID(300ull), "c");
        scene->CreateEntityWithUUID(UUID(100ull), "a");
        scene->CreateEntityWithUUID(UUID(200ull), "b");

        const auto path = dir.File("sorted.epscene");
        SceneSerializer(scene).Serialize(path);

        const json data = ParseSceneFile(path);
        const auto& entities = data["Scene"]["Entities"];
        CHECK_EQUAL(100ull, entities[0]["IDComponent"]["ID"].get<uint64_t>());
        CHECK_EQUAL(200ull, entities[1]["IDComponent"]["ID"].get<uint64_t>());
        CHECK_EQUAL(300ull, entities[2]["IDComponent"]["ID"].get<uint64_t>());
    }

    TEST(RoundTripPreservesEntities)
    {
        const TempDir dir;
        const auto original = dir.File("a.epscene");
        const auto reemitted = dir.File("b.epscene");

        // scene -> JSON(a)
        SceneSerializer(BuildCanonicalScene()).Serialize(original);

        // JSON(a) -> new scene -> JSON(b)
        Ref<Scene> loaded = CreateRef<Scene>();
        CHECK(SceneSerializer(loaded).Deserialize(original));
        SceneSerializer(loaded).Serialize(reemitted);

        // The Entities arrays (ID-sorted) must match byte-for-byte in structure.
        const json a = ParseSceneFile(original);
        const json b = ParseSceneFile(reemitted);
        CHECK(a["Scene"]["Entities"] == b["Scene"]["Entities"]);
    }

    TEST(RoundTripPreservesPointLight)
    {
        const TempDir dir;
        const auto original = dir.File("light-a.epscene");
        const auto reemitted = dir.File("light-b.epscene");

        Ref<Scene> scene = CreateRef<Scene>();
        Entity lamp = scene->CreateEntityWithUUID(UUID(400ull), "Lamp");
        auto& light = lamp.AddComponent<PointLightComponent>();
        light.Color = { 0.1f, 0.5f, 0.9f };
        light.Intensity = 12.5f;

        SceneSerializer(scene).Serialize(original);

        // The component lands in the JSON...
        const json data = ParseSceneFile(original);
        CHECK(data["Scene"]["Entities"][0].contains("PointLightComponent"));
        CHECK_CLOSE(12.5f, data["Scene"]["Entities"][0]["PointLightComponent"]["Intensity"].get<float>(), 1e-6f);

        // ...and survives a load + re-emit unchanged.
        Ref<Scene> loaded = CreateRef<Scene>();
        CHECK(SceneSerializer(loaded).Deserialize(original));
        SceneSerializer(loaded).Serialize(reemitted);
        const json b = ParseSceneFile(reemitted);
        CHECK(data["Scene"]["Entities"] == b["Scene"]["Entities"]);
    }

    TEST(SerializeEmitsEnvironment)
    {
        const TempDir dir;
        const auto path = dir.File("env.epscene");

        Ref<Scene> scene = CreateRef<Scene>();
        auto& env = scene->GetEnvironment();
        env.AmbientIntensity = 0.5f;
        env.ZenithColor = { 0.1f, 0.2f, 0.3f };
        scene->CreateEntityWithUUID(UUID(100ull), "Alpha");

        SceneSerializer(scene).Serialize(path);

        const json data = ParseSceneFile(path);
        CHECK(data["Scene"].contains("Environment"));
        CHECK_CLOSE(0.5f, data["Scene"]["Environment"]["AmbientIntensity"].get<float>(), 1e-6f);
    }

    TEST(DeserializeAppliesEnvironment)
    {
        const TempDir dir;
        const auto path = dir.File("env-rt.epscene");

        Ref<Scene> scene = CreateRef<Scene>();
        scene->GetEnvironment().AmbientIntensity = 0.25f;
        scene->GetEnvironment().GroundColor = { 0.9f, 0.8f, 0.7f };
        scene->CreateEntityWithUUID(UUID(100ull), "Alpha");
        SceneSerializer(scene).Serialize(path);

        Ref<Scene> loaded = CreateRef<Scene>();
        CHECK(SceneSerializer(loaded).Deserialize(path));

        CHECK_CLOSE(0.25f, loaded->GetEnvironment().AmbientIntensity, 1e-6f);
        CHECK_VEC3_CLOSE(glm::vec3(0.9f, 0.8f, 0.7f), loaded->GetEnvironment().GroundColor, 1e-6f);
    }

    TEST(DeserializeMissingEnvironmentKeepsDefaults)
    {
        const TempDir dir;
        const auto path = dir.File("no-env.epscene");

        // Hand-write a scene with no Environment block (as older scenes had none).
        json data;
        data["Scene"]["Name"] = "no-env";
        data["Scene"]["Handle"] = 0;
        data["Scene"]["Entities"] = json::array();
        json e;
        e["IDComponent"]["ID"] = 100ull;
        e["TagComponent"]["Tag"] = "Alpha";
        data["Scene"]["Entities"].push_back(e);
        FS::WriteText(path, data.dump(4), true);

        Ref<Scene> loaded = CreateRef<Scene>();
        CHECK(SceneSerializer(loaded).Deserialize(path));

        // The constructed defaults survive untouched.
        const EnvironmentSettings defaults;
        CHECK_CLOSE(defaults.AmbientIntensity, loaded->GetEnvironment().AmbientIntensity, 1e-6f);
        CHECK_VEC3_CLOSE(defaults.ZenithColor, loaded->GetEnvironment().ZenithColor, 1e-6f);
    }

    TEST(DeserializeMissingFileFails)
    {
        const TempDir dir;
        Ref<Scene> scene = CreateRef<Scene>();

        // A non-existent path yields an unparseable stream; Deserialize must
        // report failure rather than throw.
        CHECK(!SceneSerializer(scene).Deserialize(dir.File("does-not-exist.epscene")));
    }

    TEST(RoundTripPreservesHierarchy)
    {
        const TempDir dir;
        const auto original = dir.File("hier-a.epscene");
        const auto reemitted = dir.File("hier-b.epscene");

        Ref<Scene> scene = CreateRef<Scene>();
        Entity parent = scene->CreateEntityWithUUID(UUID(100ull), "Parent");
        Entity child = scene->CreateEntityWithUUID(UUID(200ull), "Child");
        scene->SetParent(child, parent);

        SceneSerializer(scene).Serialize(original);

        // The parent link and child list land in the JSON (entities are ID-sorted,
        // so [0] is the parent (100) and [1] is the child (200)).
        const json data = ParseSceneFile(original);
        CHECK_EQUAL(200ull, data["Scene"]["Entities"][0]["RelationshipComponent"]["Children"][0].get<uint64_t>());
        CHECK_EQUAL(100ull, data["Scene"]["Entities"][1]["RelationshipComponent"]["Parent"].get<uint64_t>());

        // ...and the links survive a load + re-emit unchanged.
        Ref<Scene> loaded = CreateRef<Scene>();
        CHECK(SceneSerializer(loaded).Deserialize(original));
        SceneSerializer(loaded).Serialize(reemitted);
        const json b = ParseSceneFile(reemitted);
        CHECK(data["Scene"]["Entities"] == b["Scene"]["Entities"]);
    }

    TEST(DeserializeRelinksParentAndChild)
    {
        const TempDir dir;
        const auto path = dir.File("relink.epscene");

        Ref<Scene> scene = CreateRef<Scene>();
        Entity parent = scene->CreateEntityWithUUID(UUID(100ull), "Parent");
        Entity child = scene->CreateEntityWithUUID(UUID(200ull), "Child");
        scene->SetParent(child, parent);
        SceneSerializer(scene).Serialize(path);

        Ref<Scene> loaded = CreateRef<Scene>();
        CHECK(SceneSerializer(loaded).Deserialize(path));

        Entity loadedChild = loaded->GetEntityByUUID(UUID(200ull));
        CHECK(static_cast<bool>(loadedChild));
        CHECK(loadedChild.GetComponent<RelationshipComponent>().Parent == UUID(100ull));

        // World composition works on the reloaded tree.
        Entity loadedParent = loaded->GetEntityByUUID(UUID(100ull));
        loadedParent.GetComponent<TransformComponent>().Translation = { 4.0f, 0.0f, 0.0f };
        const glm::vec3 world = glm::vec3(loaded->GetWorldTransform(loadedChild)[3]);
        CHECK_VEC3_CLOSE(glm::vec3(4.0f, 0.0f, 0.0f), world, 1e-5f);
    }

    TEST(DestroyEntityRemovesItFromSerialization)
    {
        const TempDir dir;
        Ref<Scene> scene = CreateRef<Scene>();
        Entity keep = scene->CreateEntityWithUUID(UUID(100ull), "Keep");
        Entity drop = scene->CreateEntityWithUUID(UUID(200ull), "Drop");

        scene->DestroyEntity(drop);

        const auto path = dir.File("after-destroy.epscene");
        SceneSerializer(scene).Serialize(path);

        const json data = ParseSceneFile(path);
        const auto& entities = data["Scene"]["Entities"];
        CHECK_EQUAL(1u, entities.size());
        CHECK_EQUAL(100ull, entities[0]["IDComponent"]["ID"].get<uint64_t>());
    }
}
