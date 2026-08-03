#include "Support/EppoTest.h"
#include "Support/GlmCheck.h"
#include "Support/TempDir.h"

#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include <glm/gtc/constants.hpp>
#include <nlohmann/json.hpp>

#include <fstream>

using namespace Eppo;

// Scene-graph consistency across a load. A scene can be stored with only one side of a
// Parent/Children link; on load such an entity is hidden from the hierarchy panel (which
// walks Children) yet still lives in the registry, so its colliders render and can never
// be selected or removed. Deserialize must repair these links.
namespace
{
    auto ParentOf(Entity entity) -> UUID
    {
        return entity.HasComponent<RelationshipComponent>() ? entity.GetComponent<RelationshipComponent>().Parent : UUID(0);
    }

    auto ChildCount(Entity entity) -> size_t
    {
        return entity.HasComponent<RelationshipComponent>() ? entity.GetComponent<RelationshipComponent>().Children.size() : 0;
    }
}

TEST(Scene, Scene_CreateEntity_DoesNotAddRelationshipComponent)
{
    const Ref<Scene> scene = CreateRef<Scene>();
    const Entity entity = scene->CreateEntity("Root");

    EXPECT_TRUE(!entity.HasComponent<RelationshipComponent>());
}

TEST(Scene, Scene_BloomSettings_UseHdrDefaults)
{
    const Ref<Scene> scene = CreateRef<Scene>();
    const auto& bloom = scene->GetBloomSettings();

    EXPECT_NEAR(0.5f, bloom.Threshold, 1e-5f);
    EXPECT_NEAR(0.25f, bloom.Knee, 1e-5f);
    EXPECT_NEAR(0.04f, bloom.Intensity, 1e-5f);
    EXPECT_NEAR(1.0f, bloom.Radius, 1e-5f);
}

TEST(Scene, SceneSerializer_BloomSettings_RoundTripsFields)
{
    const Ref<Scene> authoring = CreateRef<Scene>();
    auto& bloom = authoring->GetBloomSettings();
    bloom.Threshold = 0.35f;
    bloom.Knee = 0.15f;
    bloom.Intensity = 0.55f;
    bloom.Radius = 2.5f;

    const Testing::TempDir dir;
    const auto path = dir.File("bloom-settings.epscene");
    EP_REQUIRE(SceneSerializer(authoring).Serialize(path));

    const Ref<Scene> loaded = CreateRef<Scene>();
    EP_REQUIRE(SceneSerializer(loaded).Deserialize(path));
    const auto& loadedBloom = loaded->GetBloomSettings();
    EXPECT_NEAR(0.35f, loadedBloom.Threshold, 1e-5f);
    EXPECT_NEAR(0.15f, loadedBloom.Knee, 1e-5f);
    EXPECT_NEAR(0.55f, loadedBloom.Intensity, 1e-5f);
    EXPECT_NEAR(2.5f, loadedBloom.Radius, 1e-5f);
}

TEST(Scene, Scene_Copy_CarriesBloomSettings)
{
    const Ref<Scene> scene = CreateRef<Scene>();
    auto& bloom = scene->GetBloomSettings();
    bloom.Threshold = 0.35f;
    bloom.Knee = 0.15f;
    bloom.Intensity = 0.55f;
    bloom.Radius = 2.5f;

    const Ref<Scene> copy = Scene::Copy(scene);
    const auto& copied = copy->GetBloomSettings();
    EXPECT_NEAR(0.35f, copied.Threshold, 1e-5f);
    EXPECT_NEAR(0.15f, copied.Knee, 1e-5f);
    EXPECT_NEAR(0.55f, copied.Intensity, 1e-5f);
    EXPECT_NEAR(2.5f, copied.Radius, 1e-5f);
}

TEST(Scene, Scene_SetParent_AddsAndRemovesSparseRelationshipComponents)
{
    const Ref<Scene> scene = CreateRef<Scene>();
    const Entity parent = scene->CreateEntity("Parent");
    const Entity child = scene->CreateEntity("Child");

    scene->SetParent(child, parent);
    EXPECT_TRUE(child.HasComponent<RelationshipComponent>());
    EXPECT_TRUE(parent.HasComponent<RelationshipComponent>());
    EXPECT_EQ(static_cast<uint64_t>(parent.GetUUID()), static_cast<uint64_t>(ParentOf(child)));
    EXPECT_EQ(1u, ChildCount(parent));

    scene->SetParent(child, {});
    EXPECT_TRUE(!child.HasComponent<RelationshipComponent>());
    EXPECT_TRUE(!parent.HasComponent<RelationshipComponent>());
}

TEST(Scene, Scene_GetWorldRotation_ComposesHierarchyWithoutScale)
{
    const Ref<Scene> scene = CreateRef<Scene>();
    const Entity parent = scene->CreateEntity("Parent");
    const Entity child = scene->CreateEntity("Child");
    scene->SetParent(child, parent);

    auto& parentTransform = parent.GetComponent<TransformComponent>();
    parentTransform.Rotation.z = glm::radians(30.0f);
    parentTransform.Scale = glm::vec3(2.0f, 0.0f, -3.0f);
    child.GetComponent<TransformComponent>().Rotation.z = glm::radians(15.0f);

    const glm::vec3 direction = scene->GetWorldRotation(child) * glm::vec3(0.0f, -1.0f, 0.0f);
    const glm::vec3 expected = glm::quat(parentTransform.Rotation) *
        glm::quat(child.GetComponent<TransformComponent>().Rotation) * glm::vec3(0.0f, -1.0f, 0.0f);
    CHECK_VEC3_CLOSE(expected, direction, 1e-5f);
}

TEST(Scene, SceneSerializer_Deserialize_DetachesChildWhoseParentDoesNotListIt)
{
    const UUID parentId;
    const UUID childId;

    // Build the asymmetric link directly (child points up, parent never points down)
    // and round-trip it: the serializer emits only the child's Parent, reproducing a
    // one-directional stored link.
    const Ref<Scene> authoring = CreateRef<Scene>();
    authoring->CreateEntityWithUUID(parentId, "Parent");
    Entity child = authoring->CreateEntityWithUUID(childId, "Child");
    child.AddComponent<RelationshipComponent>().Parent = parentId;

    const Testing::TempDir dir;
    const auto path = dir.File("orphan.epscene");
    EP_REQUIRE(SceneSerializer(authoring).Serialize(path));

    const Ref<Scene> loaded = CreateRef<Scene>();
    EP_REQUIRE(SceneSerializer(loaded).Deserialize(path));

    Entity loadedChild = loaded->GetEntityByUUID(childId);
    EP_REQUIRE(static_cast<bool>(loadedChild));
    EXPECT_TRUE(!loadedChild.HasComponent<RelationshipComponent>());
    // The parent entity is untouched and still present.
    EXPECT_TRUE(static_cast<bool>(loaded->GetEntityByUUID(parentId)));
}

TEST(Scene, SceneSerializer_Deserialize_DetachPreservesWorldTransform)
{
    const UUID parentId;
    const UUID childId;
    const Ref<Scene> authoring = CreateRef<Scene>();
    Entity parent = authoring->CreateEntityWithUUID(parentId, "Parent");
    parent.GetComponent<TransformComponent>().Translation = { 4.0f, 3.0f, -2.0f };
    parent.GetComponent<TransformComponent>().Rotation = { 0.0f, glm::half_pi<float>(), 0.0f };
    parent.GetComponent<TransformComponent>().Scale = { 2.0f, 1.0f, 3.0f };

    Entity child = authoring->CreateEntityWithUUID(childId, "Child");
    child.GetComponent<TransformComponent>().Translation = { 1.0f, 2.0f, 3.0f };
    child.AddComponent<RelationshipComponent>().Parent = parentId;
    const glm::vec3 expectedWorldPosition = glm::vec3(authoring->GetWorldTransform(child)[3]);

    const Testing::TempDir dir;
    const auto path = dir.File("orphan-transform.epscene");
    EP_REQUIRE(SceneSerializer(authoring).Serialize(path));

    const Ref<Scene> loaded = CreateRef<Scene>();
    EP_REQUIRE(SceneSerializer(loaded).Deserialize(path));

    Entity loadedChild = loaded->GetEntityByUUID(childId);
    EP_REQUIRE(static_cast<bool>(loadedChild));
    EXPECT_TRUE(!loadedChild.HasComponent<RelationshipComponent>());
    const glm::vec3 loadedWorldPosition = glm::vec3(loaded->GetWorldTransform(loadedChild)[3]);
    EXPECT_NEAR(expectedWorldPosition.x, loadedWorldPosition.x, 1e-4f);
    EXPECT_NEAR(expectedWorldPosition.y, loadedWorldPosition.y, 1e-4f);
    EXPECT_NEAR(expectedWorldPosition.z, loadedWorldPosition.z, 1e-4f);
}

TEST(Scene, SceneSerializer_Deserialize_KeepsConsistentHierarchy)
{
    const UUID parentId;
    const UUID childId;

    const Ref<Scene> authoring = CreateRef<Scene>();
    Entity parent = authoring->CreateEntityWithUUID(parentId, "Parent");
    Entity child = authoring->CreateEntityWithUUID(childId, "Child");
    authoring->SetParent(child, parent); // both sides maintained

    const Testing::TempDir dir;
    const auto path = dir.File("consistent.epscene");
    EP_REQUIRE(SceneSerializer(authoring).Serialize(path));

    const Ref<Scene> loaded = CreateRef<Scene>();
    EP_REQUIRE(SceneSerializer(loaded).Deserialize(path));

    Entity loadedParent = loaded->GetEntityByUUID(parentId);
    Entity loadedChild = loaded->GetEntityByUUID(childId);
    EP_REQUIRE(static_cast<bool>(loadedParent));
    EP_REQUIRE(static_cast<bool>(loadedChild));

    // A valid relationship survives untouched.
    EXPECT_EQ(static_cast<uint64_t>(parentId), static_cast<uint64_t>(ParentOf(loadedChild)));
    EXPECT_EQ(1u, ChildCount(loadedParent));
}

TEST(Scene, SceneSerializer_Deserialize_ColliderWithMissingParentBecomesRoot)
{
    const UUID parentId;
    const UUID childId;

    const Ref<Scene> authoring = CreateRef<Scene>();
    Entity child = authoring->CreateEntityWithUUID(childId, "Child");
    child.AddComponent<BoxColliderComponent>();
    child.AddComponent<RelationshipComponent>().Parent = parentId;

    const Testing::TempDir dir;
    const auto path = dir.File("orphan-collider.epscene");
    EP_REQUIRE(SceneSerializer(authoring).Serialize(path));

    const Ref<Scene> loaded = CreateRef<Scene>();
    EP_REQUIRE(SceneSerializer(loaded).Deserialize(path));

    // The collider entity is repaired, not deleted: still present, still owns its
    // collider, now reachable as a root.
    Entity loadedChild = loaded->GetEntityByUUID(childId);
    EP_REQUIRE(static_cast<bool>(loadedChild));
    EXPECT_TRUE(loadedChild.HasComponent<BoxColliderComponent>());
    EXPECT_TRUE(!loadedChild.HasComponent<RelationshipComponent>());
}

TEST(Scene, SceneSerializer_Deserialize_InvalidParentPreservesValidChildren)
{
    const UUID missingParentId;
    const Ref<Scene> authoring = CreateRef<Scene>();
    Entity child = authoring->CreateEntity("Child");
    child.AddComponent<RelationshipComponent>().Parent = missingParentId;
    Entity grandchild = authoring->CreateEntity("Grandchild");
    authoring->SetParent(grandchild, child);

    const Testing::TempDir dir;
    const auto path = dir.File("nested-orphan.epscene");
    EP_REQUIRE(SceneSerializer(authoring).Serialize(path));

    const Ref<Scene> loaded = CreateRef<Scene>();
    EP_REQUIRE(SceneSerializer(loaded).Deserialize(path));

    Entity loadedChild = loaded->GetEntityByUUID(child.GetUUID());
    Entity loadedGrandchild = loaded->GetEntityByUUID(grandchild.GetUUID());
    EXPECT_TRUE(!static_cast<bool>(ParentOf(loadedChild)));
    EXPECT_EQ(1u, ChildCount(loadedChild));
    EXPECT_EQ(static_cast<uint64_t>(loadedChild.GetUUID()), static_cast<uint64_t>(ParentOf(loadedGrandchild)));
}

TEST(Scene, SceneSerializer_Deserialize_DeduplicatesChildren)
{
    const Ref<Scene> authoring = CreateRef<Scene>();
    Entity parent = authoring->CreateEntity("Parent");
    Entity child = authoring->CreateEntity("Child");
    authoring->SetParent(child, parent);
    parent.GetComponent<RelationshipComponent>().Children.push_back(child.GetUUID());

    const Testing::TempDir dir;
    const auto path = dir.File("duplicate-child.epscene");
    EP_REQUIRE(SceneSerializer(authoring).Serialize(path));

    const Ref<Scene> loaded = CreateRef<Scene>();
    EP_REQUIRE(SceneSerializer(loaded).Deserialize(path));

    EXPECT_EQ(1u, ChildCount(loaded->GetEntityByUUID(parent.GetUUID())));
}

TEST(Scene, Scene_DestroyEntity_CanDeleteTreeDuringEntityEnumeration)
{
    const Ref<Scene> scene = CreateRef<Scene>();
    const Entity root = scene->CreateEntity("Root");
    const Entity child = scene->CreateEntity("Child");
    const Entity leaf = scene->CreateEntity("Leaf");
    const UUID rootId = root.GetUUID();
    const UUID childId = child.GetUUID();
    const UUID leafId = leaf.GetUUID();
    scene->SetParent(child, root);

    scene->ForEachEntity(
        [&](Entity entity)
        {
            scene->DestroyEntity(entity);
        }
    );

    EXPECT_TRUE(!scene->GetEntityByUUID(rootId));
    EXPECT_TRUE(!scene->GetEntityByUUID(childId));
    EXPECT_TRUE(!scene->GetEntityByUUID(leafId));
}

TEST(Scene, Scene_DuplicateEntity_ClonesDescendantTree)
{
    const Ref<Scene> scene = CreateRef<Scene>();
    const Entity parent = scene->CreateEntity("Parent");
    const Entity root = scene->CreateEntity("Root");
    const Entity child = scene->CreateEntity("Child");
    const Entity grandchild = scene->CreateEntity("Grandchild");
    scene->SetParent(root, parent);
    scene->SetParent(child, root);
    scene->SetParent(grandchild, child);

    const Entity duplicateRoot = scene->DuplicateEntity(root);
    EP_REQUIRE(duplicateRoot);
    EXPECT_TRUE(duplicateRoot.GetUUID() != root.GetUUID());
    EXPECT_EQ(static_cast<uint64_t>(parent.GetUUID()), static_cast<uint64_t>(ParentOf(duplicateRoot)));

    const auto& rootChildren = duplicateRoot.GetComponent<RelationshipComponent>().Children;
    EP_REQUIRE_EQ(1u, rootChildren.size());
    const Entity duplicateChild = scene->GetEntityByUUID(rootChildren.front());
    EP_REQUIRE(duplicateChild);
    EXPECT_TRUE(duplicateChild.GetUUID() != child.GetUUID());
    EXPECT_EQ(static_cast<uint64_t>(duplicateRoot.GetUUID()), static_cast<uint64_t>(ParentOf(duplicateChild)));

    const auto& childChildren = duplicateChild.GetComponent<RelationshipComponent>().Children;
    EP_REQUIRE_EQ(1u, childChildren.size());
    const Entity duplicateGrandchild = scene->GetEntityByUUID(childChildren.front());
    EP_REQUIRE(duplicateGrandchild);
    EXPECT_TRUE(duplicateGrandchild.GetUUID() != grandchild.GetUUID());
    EXPECT_EQ(static_cast<uint64_t>(duplicateChild.GetUUID()), static_cast<uint64_t>(ParentOf(duplicateGrandchild)));
}

// The committed harness scene must round-trip through JSON serialization intact.
// Scene has no entity enumeration, so read the result back through the serializer.
TEST(Scene, SceneSerializer_HarnessScene_LoadsThreeEntities)
{
    const auto scenePath = FS::GetExecutableDirectory() / "TestData" / "Scenes" / "harness.epscene";
    EP_REQUIRE(FS::Exists(scenePath));

    const Ref<Scene> scene = CreateRef<Scene>();
    EP_REQUIRE(SceneSerializer(scene).Deserialize(scenePath));

    const Testing::TempDir dir;
    const auto out = dir.File("readback.epscene");
    EP_REQUIRE(SceneSerializer(scene).Serialize(out));

    std::ifstream stream(out);
    const nlohmann::json data = nlohmann::json::parse(stream);
    const auto& entities = data["Scene"]["Entities"];

    EXPECT_EQ(3u, entities.size());
    // Entities are ID-sorted (1001 Ground, 1002 Player, 1003 Prop).
    EXPECT_EQ(std::string("Ground"), entities[0]["TagComponent"]["Tag"].get<std::string>());
    EXPECT_EQ(std::string("Player"), entities[1]["TagComponent"]["Tag"].get<std::string>());
    EXPECT_EQ(1003ull, entities[2]["IDComponent"]["ID"].get<uint64_t>());
}

TEST(Scene, SceneSerializer_DirectionalLightComponent_RoundTripsFieldsWithoutDirection)
{
    const UUID sunId;
    const Ref<Scene> authoring = CreateRef<Scene>();
    Entity sun = authoring->CreateEntityWithUUID(sunId, "Sun");
    auto& light = sun.AddComponent<DirectionalLightComponent>();
    light.Color = { 0.9f, 0.4f, 0.1f };
    light.Intensity = 2.5f;
    sun.GetComponent<TransformComponent>().Rotation = { 0.1f, 0.2f, 0.3f };

    const Testing::TempDir dir;
    const auto path = dir.File("directional-light.epscene");
    EP_REQUIRE(SceneSerializer(authoring).Serialize(path));

    std::ifstream stream(path);
    const nlohmann::json data = nlohmann::json::parse(stream);
    const auto& serializedLight = data["Scene"]["Entities"][0]["DirectionalLightComponent"];
    EXPECT_TRUE(!serializedLight.contains("Direction"));

    const Ref<Scene> loaded = CreateRef<Scene>();
    EP_REQUIRE(SceneSerializer(loaded).Deserialize(path));

    Entity loadedSun = loaded->GetEntityByUUID(sunId);
    EP_REQUIRE(static_cast<bool>(loadedSun));
    EP_REQUIRE(loadedSun.HasComponent<DirectionalLightComponent>());
    const auto& loadedLight = loadedSun.GetComponent<DirectionalLightComponent>();
    CHECK_VEC3_CLOSE(glm::vec3(0.9f, 0.4f, 0.1f), loadedLight.Color, 1e-5f);
    EXPECT_NEAR(2.5f, loadedLight.Intensity, 1e-5f);
    CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), loadedSun.GetComponent<TransformComponent>().Rotation, 1e-5f);
}

TEST(Scene, Scene_DuplicateEntity_CarriesDirectionalLightComponent)
{
    const Ref<Scene> scene = CreateRef<Scene>();
    Entity sun = scene->CreateEntity("Sun");
    auto& light = sun.AddComponent<DirectionalLightComponent>();
    light.Color = { 0.9f, 0.4f, 0.1f };
    light.Intensity = 2.5f;

    const Entity duplicate = scene->DuplicateEntity(sun);
    EP_REQUIRE(duplicate);
    EXPECT_TRUE(duplicate.GetUUID() != sun.GetUUID());
    EP_REQUIRE(duplicate.HasComponent<DirectionalLightComponent>());
    const auto& copied = duplicate.GetComponent<DirectionalLightComponent>();
    CHECK_VEC3_CLOSE(glm::vec3(0.9f, 0.4f, 0.1f), copied.Color, 1e-5f);
    EXPECT_NEAR(2.5f, copied.Intensity, 1e-5f);
}

TEST(Scene, Scene_Copy_CarriesDirectionalLightComponent)
{
    const UUID sunId;
    const Ref<Scene> scene = CreateRef<Scene>();
    Entity sun = scene->CreateEntityWithUUID(sunId, "Sun");
    auto& light = sun.AddComponent<DirectionalLightComponent>();
    light.Color = { 0.9f, 0.4f, 0.1f };
    light.Intensity = 2.5f;

    const Ref<Scene> copy = Scene::Copy(scene);
    Entity copiedSun = copy->GetEntityByUUID(sunId);
    EP_REQUIRE(static_cast<bool>(copiedSun));
    EP_REQUIRE(copiedSun.HasComponent<DirectionalLightComponent>());
    const auto& copied = copiedSun.GetComponent<DirectionalLightComponent>();
    CHECK_VEC3_CLOSE(glm::vec3(0.9f, 0.4f, 0.1f), copied.Color, 1e-5f);
    EXPECT_NEAR(2.5f, copied.Intensity, 1e-5f);
}
