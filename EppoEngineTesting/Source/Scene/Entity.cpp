#include "Support/EppoTest.h"

#include "Scene/Entity.h"
#include "Scene/Scene.h"

using namespace Eppo;

#define ValidateDefaultEntity(e)                                                                                                           \
EXPECT_EQ("Entity", e.GetName());                                                                                                    \
EXPECT_TRUE(e.HasComponent<IDComponent>());                                                                                                  \
EXPECT_TRUE(e.HasComponent<TagComponent>());                                                                                                 \
EXPECT_TRUE(e.HasComponent<TransformComponent>());

TEST(Scene, Entity_DefaultConstructorCreatesInvalidEntity)
{
    Entity entity;
    EXPECT_TRUE(!entity);
}

TEST(Scene, Entity_ParamConstructorCreatesValidEntity)
{
    const auto scene = CreateRef<Scene>();
    EP_REQUIRE(scene);

    // Because we trigger the constructor through the scene,
    // We end up with a more complete entity than the constructor alone would give
    // We shall only verify the constructor relevant things here
    const auto entity = scene->CreateEntity();
    EXPECT_EQ("Entity", entity.GetName());
}

TEST(Scene, Entity_HasComponent)
{
    const auto scene = CreateRef<Scene>();
    EP_REQUIRE(scene);

    auto entity = scene->CreateEntity();
    ValidateDefaultEntity(entity);

    entity.AddComponent<RelationshipComponent>();
    EXPECT_TRUE(entity.HasComponent<RelationshipComponent>());

    (void)entity.RemoveComponent<RelationshipComponent>();
    EXPECT_TRUE(!entity.HasComponent<RelationshipComponent>());
}

TEST(Scene, Entity_AddComponent)
{
    const auto scene = CreateRef<Scene>();
    EP_REQUIRE(scene);

    auto entity = scene->CreateEntity();
    ValidateDefaultEntity(entity);

    entity.AddComponent<RelationshipComponent>();
    EXPECT_TRUE(entity.HasComponent<RelationshipComponent>());
}

TEST(Scene, Entity_TryAddComponent_ReturnsExistingComponent)
{
    const auto scene = CreateRef<Scene>();
    EP_REQUIRE(scene);

    auto entity = scene->CreateEntity();
    ValidateDefaultEntity(entity);

    auto& tag = entity.GetComponent<TagComponent>().Tag;
    tag = "Works";

    auto& result = entity.TryAddComponent<TagComponent>().Tag;
    EXPECT_EQ("Works", result);
}
