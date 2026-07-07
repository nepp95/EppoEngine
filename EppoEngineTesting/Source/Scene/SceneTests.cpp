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
}
