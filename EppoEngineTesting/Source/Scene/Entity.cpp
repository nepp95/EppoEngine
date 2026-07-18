#include "Support/EppoTest.h"

#include "Scene/Entity.h"
#include "Scene/Scene.h"

using namespace Eppo;

SUITE(Scene)
{
    #define ValidateDefaultEntity(e) \
        CHECK_EQUAL("Entity", e.GetName()); \
        CHECK(e.HasComponent<IDComponent>()); \
        CHECK(e.HasComponent<TagComponent>()); \
        CHECK(e.HasComponent<TransformComponent>());

    TEST(Entity_DefaultConstructorCreatesInvalidEntity)
    {
        Entity entity;
        CHECK(!entity);
    }

    TEST(Entity_ParamConstructorCreatesValidEntity)
    {
        const auto scene = CreateRef<Scene>();
        REQUIRE CHECK(scene);

        // Because we trigger the constructor through the scene,
        // We end up with a more complete entity than the constructor alone would give
        // We shall only verify the constructor relevant things here
        const auto entity = scene->CreateEntity();
        CHECK_EQUAL("Entity", entity.GetName());
    }

    TEST(Entity_HasComponent)
    {
        const auto scene = CreateRef<Scene>();
        REQUIRE CHECK(scene);

        auto entity = scene->CreateEntity();
        ValidateDefaultEntity(entity);

        entity.AddComponent<RelationshipComponent>();
        CHECK(entity.HasComponent<RelationshipComponent>());

        (void)entity.RemoveComponent<RelationshipComponent>();
        CHECK(!entity.HasComponent<RelationshipComponent>());
    }

    TEST(Entity_AddComponent)
    {
        const auto scene = CreateRef<Scene>();
        REQUIRE CHECK(scene);

        auto entity = scene->CreateEntity();
        ValidateDefaultEntity(entity);

        entity.AddComponent<RelationshipComponent>();
        CHECK(entity.HasComponent<RelationshipComponent>());
    }

    TEST(Entity_TryAddComponent_ReturnsExistingComponent)
    {
        const auto scene = CreateRef<Scene>();
        REQUIRE CHECK(scene);

        auto entity = scene->CreateEntity();
        ValidateDefaultEntity(entity);

        auto& tag = entity.GetComponent<TagComponent>().Tag;
        tag = "Works";

        auto& result = entity.TryAddComponent<TagComponent>().Tag;
        CHECK_EQUAL("Works", result);
    }
}
