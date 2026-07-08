#include "Support/EppoTest.h"
#include "Support/GlmCheck.h"

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"

using namespace Eppo;

// Scene owns a headless EnTT registry; entity creation, component access, and
// duplication need no renderer or GPU. Scene exposes no public entity
// enumeration, so these tests assert through the Entity handles that the CRUD
// calls return (registry-wide effects are covered via serialization in
// SceneSerializerTests).
SUITE(Scene)
{
    TEST(CreateEntityHasCoreComponents)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Player");

        CHECK(static_cast<bool>(entity));
        CHECK(entity.HasComponent<IDComponent>());
        CHECK(entity.HasComponent<TransformComponent>());
        CHECK(entity.HasComponent<TagComponent>());
    }

    TEST(CreateEntityDefaultsTagWhenNameEmpty)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity();

        CHECK_EQUAL(std::string("Entity"), entity.GetName());
    }

    TEST(CreateEntityPreservesCustomName)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Camera");

        CHECK_EQUAL(std::string("Camera"), entity.GetName());
    }

    TEST(CreateEntityDrawsNonReservedUUID)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity();

        // CreateEntity delegates to a default UUID, which reserves 1-99.
        CHECK(static_cast<uint64_t>(entity.GetUUID()) >= 100);
    }

    TEST(CreateEntityWithUUIDPreservesId)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        const UUID id(555ull);
        Entity entity = scene->CreateEntityWithUUID(id, "Fixed");

        CHECK(entity.GetUUID() == id);
        CHECK_EQUAL(std::string("Fixed"), entity.GetName());
    }

    TEST(DistinctEntitiesHaveDistinctUUIDs)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity a = scene->CreateEntity("A");
        Entity b = scene->CreateEntity("B");

        CHECK(a.GetUUID() != b.GetUUID());
        CHECK(a != b);
    }

    TEST(AddAndReadComponent)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity();

        CHECK(!entity.HasComponent<MeshComponent>());
        entity.AddComponent<MeshComponent>(AssetHandle(4242ull));
        CHECK(entity.HasComponent<MeshComponent>());
        CHECK_EQUAL(4242ull, static_cast<uint64_t>(entity.GetComponent<MeshComponent>().MeshHandle));
    }

    TEST(RemoveComponent)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity();
        entity.AddComponent<MeshComponent>(AssetHandle(1ull));

        entity.RemoveComponent<MeshComponent>();
        CHECK(!entity.HasComponent<MeshComponent>());
    }

    TEST(DuplicateEntityCopiesComponentsUnderNewId)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity source = scene->CreateEntity("Original");
        source.GetComponent<TransformComponent>().Translation = { 3.0f, 4.0f, 5.0f };
        source.AddComponent<MeshComponent>(AssetHandle(77ull));

        Entity copy = scene->DuplicateEntity(source);

        // New identity, same name, copied component values.
        CHECK(copy.GetUUID() != source.GetUUID());
        CHECK_EQUAL(std::string("Original"), copy.GetName());
        CHECK_VEC3_CLOSE(glm::vec3(3.0f, 4.0f, 5.0f), copy.GetComponent<TransformComponent>().Translation, 1e-6f);
        CHECK(copy.HasComponent<MeshComponent>());
        CHECK_EQUAL(77ull, static_cast<uint64_t>(copy.GetComponent<MeshComponent>().MeshHandle));
    }

    TEST(DuplicateEntityCopiesPointLight)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity source = scene->CreateEntity("Lamp");
        auto& light = source.AddComponent<PointLightComponent>();
        light.Color = { 0.2f, 0.4f, 0.8f };
        light.Intensity = 25.0f;

        Entity copy = scene->DuplicateEntity(source);

        CHECK(copy.HasComponent<PointLightComponent>());
        CHECK_VEC3_CLOSE(glm::vec3(0.2f, 0.4f, 0.8f), copy.GetComponent<PointLightComponent>().Color, 1e-6f);
        CHECK_CLOSE(25.0f, copy.GetComponent<PointLightComponent>().Intensity, 1e-6f);
    }

    TEST(NewEntityStartsAsRootWithNoChildren)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Root");

        CHECK(entity.HasComponent<RelationshipComponent>());
        CHECK(!entity.GetComponent<RelationshipComponent>().Parent);
        CHECK(entity.GetComponent<RelationshipComponent>().Children.empty());
    }

    TEST(SetParentEstablishesLinks)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");

        scene->SetParent(child, parent);

        CHECK(child.GetComponent<RelationshipComponent>().Parent == parent.GetUUID());
        const auto& kids = parent.GetComponent<RelationshipComponent>().Children;
        CHECK_EQUAL(1u, kids.size());
        CHECK(kids[0] == child.GetUUID());
    }

    TEST(ReparentMovesChildBetweenParents)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity first = scene->CreateEntity("First");
        Entity second = scene->CreateEntity("Second");
        Entity child = scene->CreateEntity("Child");

        scene->SetParent(child, first);
        scene->SetParent(child, second);

        CHECK(child.GetComponent<RelationshipComponent>().Parent == second.GetUUID());
        CHECK(first.GetComponent<RelationshipComponent>().Children.empty());
        CHECK_EQUAL(1u, second.GetComponent<RelationshipComponent>().Children.size());
    }

    TEST(UnparentReturnsChildToRoot)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");
        scene->SetParent(child, parent);

        scene->SetParent(child, {});

        CHECK(!child.GetComponent<RelationshipComponent>().Parent);
        CHECK(parent.GetComponent<RelationshipComponent>().Children.empty());
    }

    TEST(GetWorldTransformComposesParentChain)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity parent = scene->CreateEntity("Parent");
        parent.GetComponent<TransformComponent>().Translation = { 10.0f, 0.0f, 0.0f };
        Entity child = scene->CreateEntity("Child");

        scene->SetParent(child, parent);
        // Author a local offset under the parent after linking.
        child.GetComponent<TransformComponent>().Translation = { 0.0f, 5.0f, 0.0f };

        const glm::vec3 world = glm::vec3(scene->GetWorldTransform(child)[3]);
        CHECK_VEC3_CLOSE(glm::vec3(10.0f, 5.0f, 0.0f), world, 1e-5f);
    }

    TEST(ReparentPreservesWorldPosition)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity parent = scene->CreateEntity("Parent");
        parent.GetComponent<TransformComponent>().Translation = { 10.0f, 0.0f, 0.0f };

        Entity child = scene->CreateEntity("Child");
        child.GetComponent<TransformComponent>().Translation = { 3.0f, 0.0f, 0.0f };

        scene->SetParent(child, parent);

        // World position is unchanged...
        const glm::vec3 world = glm::vec3(scene->GetWorldTransform(child)[3]);
        CHECK_VEC3_CLOSE(glm::vec3(3.0f, 0.0f, 0.0f), world, 1e-5f);
        // ...achieved by re-solving the local translation against the new parent.
        CHECK_VEC3_CLOSE(glm::vec3(-7.0f, 0.0f, 0.0f), child.GetComponent<TransformComponent>().Translation, 1e-5f);
    }

    TEST(SetParentRejectsCycle)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");
        scene->SetParent(child, parent);

        // Making the parent a child of its own child would form a cycle: ignored.
        scene->SetParent(parent, child);

        CHECK(!parent.GetComponent<RelationshipComponent>().Parent);
        CHECK(child.GetComponent<RelationshipComponent>().Parent == parent.GetUUID());
    }

    TEST(DestroyEntityRemovesWholeSubtree)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity root = scene->CreateEntity("Root");
        Entity child = scene->CreateEntity("Child");
        Entity grandchild = scene->CreateEntity("Grandchild");
        scene->SetParent(child, root);
        scene->SetParent(grandchild, child);

        const UUID childId = child.GetUUID();
        const UUID grandchildId = grandchild.GetUUID();

        scene->DestroyEntity(root);

        CHECK(!scene->GetEntityByUUID(childId));
        CHECK(!scene->GetEntityByUUID(grandchildId));
    }

    TEST(DestroyChildDetachesFromParent)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");
        scene->SetParent(child, parent);

        scene->DestroyEntity(child);

        CHECK(parent.GetComponent<RelationshipComponent>().Children.empty());
    }
}
