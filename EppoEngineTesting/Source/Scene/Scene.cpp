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
SUITE(Scene)
{
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

    TEST(Scene_CreateEntity_DoesNotAddRelationshipComponent)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        const Entity entity = scene->CreateEntity("Root");

        CHECK(!entity.HasComponent<RelationshipComponent>());
    }

    TEST(Scene_SetParent_AddsAndRemovesSparseRelationshipComponents)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        const Entity parent = scene->CreateEntity("Parent");
        const Entity child = scene->CreateEntity("Child");

        scene->SetParent(child, parent);
        CHECK(child.HasComponent<RelationshipComponent>());
        CHECK(parent.HasComponent<RelationshipComponent>());
        CHECK_EQUAL(static_cast<uint64_t>(parent.GetUUID()), static_cast<uint64_t>(ParentOf(child)));
        CHECK_EQUAL(1u, ChildCount(parent));

        scene->SetParent(child, {});
        CHECK(!child.HasComponent<RelationshipComponent>());
        CHECK(!parent.HasComponent<RelationshipComponent>());
    }

    TEST(SceneSerializer_Deserialize_DetachesChildWhoseParentDoesNotListIt)
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
        REQUIRE CHECK(SceneSerializer(authoring).Serialize(path));

        const Ref<Scene> loaded = CreateRef<Scene>();
        REQUIRE CHECK(SceneSerializer(loaded).Deserialize(path));

        Entity loadedChild = loaded->GetEntityByUUID(childId);
        REQUIRE CHECK(static_cast<bool>(loadedChild));
        CHECK(!loadedChild.HasComponent<RelationshipComponent>());
        // The parent entity is untouched and still present.
        CHECK(static_cast<bool>(loaded->GetEntityByUUID(parentId)));
    }

    TEST(SceneSerializer_Deserialize_DetachPreservesWorldTransform)
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
        REQUIRE CHECK(SceneSerializer(authoring).Serialize(path));

        const Ref<Scene> loaded = CreateRef<Scene>();
        REQUIRE CHECK(SceneSerializer(loaded).Deserialize(path));

        Entity loadedChild = loaded->GetEntityByUUID(childId);
        REQUIRE CHECK(static_cast<bool>(loadedChild));
        CHECK(!loadedChild.HasComponent<RelationshipComponent>());
        const glm::vec3 loadedWorldPosition = glm::vec3(loaded->GetWorldTransform(loadedChild)[3]);
        CHECK_CLOSE(expectedWorldPosition.x, loadedWorldPosition.x, 1e-4f);
        CHECK_CLOSE(expectedWorldPosition.y, loadedWorldPosition.y, 1e-4f);
        CHECK_CLOSE(expectedWorldPosition.z, loadedWorldPosition.z, 1e-4f);
    }

    TEST(SceneSerializer_Deserialize_KeepsConsistentHierarchy)
    {
        const UUID parentId;
        const UUID childId;

        const Ref<Scene> authoring = CreateRef<Scene>();
        Entity parent = authoring->CreateEntityWithUUID(parentId, "Parent");
        Entity child = authoring->CreateEntityWithUUID(childId, "Child");
        authoring->SetParent(child, parent); // both sides maintained

        const Testing::TempDir dir;
        const auto path = dir.File("consistent.epscene");
        REQUIRE CHECK(SceneSerializer(authoring).Serialize(path));

        const Ref<Scene> loaded = CreateRef<Scene>();
        REQUIRE CHECK(SceneSerializer(loaded).Deserialize(path));

        Entity loadedParent = loaded->GetEntityByUUID(parentId);
        Entity loadedChild = loaded->GetEntityByUUID(childId);
        REQUIRE CHECK(static_cast<bool>(loadedParent));
        REQUIRE CHECK(static_cast<bool>(loadedChild));

        // A valid relationship survives untouched.
        CHECK_EQUAL(static_cast<uint64_t>(parentId), static_cast<uint64_t>(ParentOf(loadedChild)));
        CHECK_EQUAL(1u, ChildCount(loadedParent));
    }

    TEST(SceneSerializer_Deserialize_ColliderWithMissingParentBecomesRoot)
    {
        const UUID parentId;
        const UUID childId;

        const Ref<Scene> authoring = CreateRef<Scene>();
        Entity child = authoring->CreateEntityWithUUID(childId, "Child");
        child.AddComponent<BoxColliderComponent>();
        child.AddComponent<RelationshipComponent>().Parent = parentId;

        const Testing::TempDir dir;
        const auto path = dir.File("orphan-collider.epscene");
        REQUIRE CHECK(SceneSerializer(authoring).Serialize(path));

        const Ref<Scene> loaded = CreateRef<Scene>();
        REQUIRE CHECK(SceneSerializer(loaded).Deserialize(path));

        // The collider entity is repaired, not deleted: still present, still owns its
        // collider, now reachable as a root.
        Entity loadedChild = loaded->GetEntityByUUID(childId);
        REQUIRE CHECK(static_cast<bool>(loadedChild));
        CHECK(loadedChild.HasComponent<BoxColliderComponent>());
        CHECK(!loadedChild.HasComponent<RelationshipComponent>());
    }

    TEST(SceneSerializer_Deserialize_InvalidParentPreservesValidChildren)
    {
        const UUID missingParentId;
        const Ref<Scene> authoring = CreateRef<Scene>();
        Entity child = authoring->CreateEntity("Child");
        child.AddComponent<RelationshipComponent>().Parent = missingParentId;
        Entity grandchild = authoring->CreateEntity("Grandchild");
        authoring->SetParent(grandchild, child);

        const Testing::TempDir dir;
        const auto path = dir.File("nested-orphan.epscene");
        REQUIRE CHECK(SceneSerializer(authoring).Serialize(path));

        const Ref<Scene> loaded = CreateRef<Scene>();
        REQUIRE CHECK(SceneSerializer(loaded).Deserialize(path));

        Entity loadedChild = loaded->GetEntityByUUID(child.GetUUID());
        Entity loadedGrandchild = loaded->GetEntityByUUID(grandchild.GetUUID());
        CHECK(!static_cast<bool>(ParentOf(loadedChild)));
        CHECK_EQUAL(1u, ChildCount(loadedChild));
        CHECK_EQUAL(static_cast<uint64_t>(loadedChild.GetUUID()), static_cast<uint64_t>(ParentOf(loadedGrandchild)));
    }

    TEST(SceneSerializer_Deserialize_DeduplicatesChildren)
    {
        const Ref<Scene> authoring = CreateRef<Scene>();
        Entity parent = authoring->CreateEntity("Parent");
        Entity child = authoring->CreateEntity("Child");
        authoring->SetParent(child, parent);
        parent.GetComponent<RelationshipComponent>().Children.push_back(child.GetUUID());

        const Testing::TempDir dir;
        const auto path = dir.File("duplicate-child.epscene");
        REQUIRE CHECK(SceneSerializer(authoring).Serialize(path));

        const Ref<Scene> loaded = CreateRef<Scene>();
        REQUIRE CHECK(SceneSerializer(loaded).Deserialize(path));

        CHECK_EQUAL(1u, ChildCount(loaded->GetEntityByUUID(parent.GetUUID())));
    }

    TEST(Scene_DestroyEntity_CanDeleteTreeDuringEntityEnumeration)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        const Entity root = scene->CreateEntity("Root");
        const Entity child = scene->CreateEntity("Child");
        const Entity leaf = scene->CreateEntity("Leaf");
        const UUID rootId = root.GetUUID();
        const UUID childId = child.GetUUID();
        const UUID leafId = leaf.GetUUID();
        scene->SetParent(child, root);

        scene->ForEachEntity([&](Entity entity)
        {
            scene->DestroyEntity(entity);
        });

        CHECK(!scene->GetEntityByUUID(rootId));
        CHECK(!scene->GetEntityByUUID(childId));
        CHECK(!scene->GetEntityByUUID(leafId));
    }

    TEST(Scene_DuplicateEntity_ClonesDescendantTree)
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
        REQUIRE CHECK(duplicateRoot);
        CHECK(duplicateRoot.GetUUID() != root.GetUUID());
        CHECK_EQUAL(static_cast<uint64_t>(parent.GetUUID()), static_cast<uint64_t>(ParentOf(duplicateRoot)));

        const auto& rootChildren = duplicateRoot.GetComponent<RelationshipComponent>().Children;
        REQUIRE CHECK_EQUAL(1u, rootChildren.size());
        const Entity duplicateChild = scene->GetEntityByUUID(rootChildren.front());
        REQUIRE CHECK(duplicateChild);
        CHECK(duplicateChild.GetUUID() != child.GetUUID());
        CHECK_EQUAL(static_cast<uint64_t>(duplicateRoot.GetUUID()), static_cast<uint64_t>(ParentOf(duplicateChild)));

        const auto& childChildren = duplicateChild.GetComponent<RelationshipComponent>().Children;
        REQUIRE CHECK_EQUAL(1u, childChildren.size());
        const Entity duplicateGrandchild = scene->GetEntityByUUID(childChildren.front());
        REQUIRE CHECK(duplicateGrandchild);
        CHECK(duplicateGrandchild.GetUUID() != grandchild.GetUUID());
        CHECK_EQUAL(static_cast<uint64_t>(duplicateChild.GetUUID()), static_cast<uint64_t>(ParentOf(duplicateGrandchild)));
    }

    // The committed harness scene must round-trip through JSON serialization intact.
    // Scene has no entity enumeration, so read the result back through the serializer.
    TEST(SceneSerializer_HarnessScene_LoadsThreeEntities)
    {
        const auto scenePath = FS::GetRootDirectory() / "TestData" / "Scenes" / "harness.epscene";
        REQUIRE CHECK(FS::Exists(scenePath));

        const Ref<Scene> scene = CreateRef<Scene>();
        REQUIRE CHECK(SceneSerializer(scene).Deserialize(scenePath));

        const Testing::TempDir dir;
        const auto out = dir.File("readback.epscene");
        REQUIRE CHECK(SceneSerializer(scene).Serialize(out));

        std::ifstream stream(out);
        const nlohmann::json data = nlohmann::json::parse(stream);
        const auto& entities = data["Scene"]["Entities"];

        CHECK_EQUAL(3u, entities.size());
        // Entities are ID-sorted (1001 Ground, 1002 Player, 1003 Prop).
        CHECK_EQUAL(std::string("Ground"), entities[0]["TagComponent"]["Tag"].get<std::string>());
        CHECK_EQUAL(std::string("Player"), entities[1]["TagComponent"]["Tag"].get<std::string>());
        CHECK_EQUAL(1003ull, entities[2]["IDComponent"]["ID"].get<uint64_t>());
    }

    // Direction is stored verbatim (unnormalized): only SubmitDirectionalLight normalizes,
    // so the persisted/copied component must carry the authored vector as-is.
    TEST(SceneSerializer_DirectionalLightComponent_RoundTripsFields)
    {
        const UUID sunId;
        const Ref<Scene> authoring = CreateRef<Scene>();
        Entity sun = authoring->CreateEntityWithUUID(sunId, "Sun");
        auto& light = sun.AddComponent<DirectionalLightComponent>();
        light.Direction = { 0.3f, -0.6f, 0.2f };
        light.Color = { 0.9f, 0.4f, 0.1f };
        light.Intensity = 2.5f;

        const Testing::TempDir dir;
        const auto path = dir.File("directional-light.epscene");
        REQUIRE CHECK(SceneSerializer(authoring).Serialize(path));

        const Ref<Scene> loaded = CreateRef<Scene>();
        REQUIRE CHECK(SceneSerializer(loaded).Deserialize(path));

        Entity loadedSun = loaded->GetEntityByUUID(sunId);
        REQUIRE CHECK(static_cast<bool>(loadedSun));
        REQUIRE CHECK(loadedSun.HasComponent<DirectionalLightComponent>());
        const auto& loadedLight = loadedSun.GetComponent<DirectionalLightComponent>();
        CHECK_VEC3_CLOSE(glm::vec3(0.3f, -0.6f, 0.2f), loadedLight.Direction, 1e-5f);
        CHECK_VEC3_CLOSE(glm::vec3(0.9f, 0.4f, 0.1f), loadedLight.Color, 1e-5f);
        CHECK_CLOSE(2.5f, loadedLight.Intensity, 1e-5f);
    }

    TEST(Scene_DuplicateEntity_CarriesDirectionalLightComponent)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity sun = scene->CreateEntity("Sun");
        auto& light = sun.AddComponent<DirectionalLightComponent>();
        light.Direction = { 0.3f, -0.6f, 0.2f };
        light.Color = { 0.9f, 0.4f, 0.1f };
        light.Intensity = 2.5f;

        const Entity duplicate = scene->DuplicateEntity(sun);
        REQUIRE CHECK(duplicate);
        CHECK(duplicate.GetUUID() != sun.GetUUID());
        REQUIRE CHECK(duplicate.HasComponent<DirectionalLightComponent>());
        const auto& copied = duplicate.GetComponent<DirectionalLightComponent>();
        CHECK_VEC3_CLOSE(glm::vec3(0.3f, -0.6f, 0.2f), copied.Direction, 1e-5f);
        CHECK_VEC3_CLOSE(glm::vec3(0.9f, 0.4f, 0.1f), copied.Color, 1e-5f);
        CHECK_CLOSE(2.5f, copied.Intensity, 1e-5f);
    }

    TEST(Scene_Copy_CarriesDirectionalLightComponent)
    {
        const UUID sunId;
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity sun = scene->CreateEntityWithUUID(sunId, "Sun");
        auto& light = sun.AddComponent<DirectionalLightComponent>();
        light.Direction = { 0.3f, -0.6f, 0.2f };
        light.Color = { 0.9f, 0.4f, 0.1f };
        light.Intensity = 2.5f;

        const Ref<Scene> copy = Scene::Copy(scene);
        Entity copiedSun = copy->GetEntityByUUID(sunId);
        REQUIRE CHECK(static_cast<bool>(copiedSun));
        REQUIRE CHECK(copiedSun.HasComponent<DirectionalLightComponent>());
        const auto& copied = copiedSun.GetComponent<DirectionalLightComponent>();
        CHECK_VEC3_CLOSE(glm::vec3(0.3f, -0.6f, 0.2f), copied.Direction, 1e-5f);
        CHECK_VEC3_CLOSE(glm::vec3(0.9f, 0.4f, 0.1f), copied.Color, 1e-5f);
        CHECK_CLOSE(2.5f, copied.Intensity, 1e-5f);
    }
}
