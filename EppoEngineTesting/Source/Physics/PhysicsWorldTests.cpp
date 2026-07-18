#include "Support/EppoTest.h"
#include "Support/GlmCheck.h"
#include "Support/TempDir.h"

#include "Physics/PhysicsWorld.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

using namespace Eppo;

// Box3D world in isolation, plus scene-runtime physics and component
// serialization further down — all headless ('unit' suite).
SUITE(Physics)
{
    namespace
    {
        auto MakeBody(PhysicsWorld& world, const UUID id, RigidBodyComponent::BodyType type, const glm::vec3& position) -> void
        {
            RigidBodyComponent rb;
            rb.Type = type;

            TransformComponent tc;
            tc.Translation = position;

            world.CreateBody(id, rb, tc, { ColliderData{} }); // default ColliderData: unit-ish box, density 1
        }

        auto MakeDynamic(PhysicsWorld& world, const UUID id, const glm::vec3& position) -> void
        {
            MakeBody(world, id, RigidBodyComponent::BodyType::Dynamic, position);
        }

        // Steps a scene's runtime physics for a fixed number of frames.
        auto StepScene(const Ref<Scene>& scene, const int frames) -> void
        {
            for (int i = 0; i < frames; ++i)
                scene->OnUpdateRuntime(1.0f / 60.0f);
        }
    }

    TEST(PhysicsWorld_DynamicBody_FallsUnderGravity)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID id;
        MakeDynamic(world, id, { 0.0f, 10.0f, 0.0f });

        const float startY = world.GetPosition(id).y;
        for (int i = 0; i < 60; ++i)
            world.Step(1.0f / 60.0f);

        CHECK(world.GetPosition(id).y < startY - 0.1f);
    }

    TEST(PhysicsWorld_ApplyLinearImpulse_AddsVelocity)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f }); // gravity-free so only the impulse acts

        const UUID id;
        MakeDynamic(world, id, glm::vec3(0.0f));

        world.ApplyLinearImpulse(id, { 5.0f, 0.0f, 0.0f });
        world.Step(1.0f / 60.0f);

        CHECK(world.GetLinearVelocity(id).x > 0.0f);
    }

    TEST(PhysicsWorld_StaticBody_DoesNotMove)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID id;
        MakeBody(world, id, RigidBodyComponent::BodyType::Static, { 0.0f, 5.0f, 0.0f });

        for (int i = 0; i < 30; ++i)
            world.Step(1.0f / 60.0f);

        CHECK_CLOSE(5.0f, world.GetPosition(id).y, 0.001f);
    }

    TEST(PhysicsWorld_UnknownEntity_AccessorsAreSafe)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID missing;
        CHECK(!world.HasBody(missing));
        // Must not crash; velocity of an unknown body reads as zero.
        world.ApplyLinearImpulse(missing, { 1.0f, 0.0f, 0.0f });
        CHECK_CLOSE(0.0f, world.GetLinearVelocity(missing).x, 0.001f);
    }

    TEST(PhysicsWorld_SphereCollider_FallsUnderGravity)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID id;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;

            TransformComponent tc;
            tc.Translation = { 0.0f, 10.0f, 0.0f };

            ColliderData collider;
            collider.Shape = ColliderShape::Sphere;
            collider.Radius = 0.5f;

            world.CreateBody(id, rb, tc, { collider });
        }

        const float startY = world.GetPosition(id).y;
        for (int i = 0; i < 60; ++i)
            world.Step(1.0f / 60.0f);

        CHECK(world.GetPosition(id).y < startY - 0.1f);
    }

    TEST(PhysicsWorld_CapsuleCollider_FallsUnderGravity)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID id;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;

            TransformComponent tc;
            tc.Translation = { 0.0f, 10.0f, 0.0f };

            ColliderData collider;
            collider.Shape = ColliderShape::Capsule;
            collider.Radius = 0.5f;
            collider.Height = 1.0f;

            world.CreateBody(id, rb, tc, { collider });
        }

        const float startY = world.GetPosition(id).y;
        for (int i = 0; i < 60; ++i)
            world.Step(1.0f / 60.0f);

        CHECK(world.GetPosition(id).y < startY - 0.1f);
    }

    TEST(PhysicsWorld_Rotation_PreservedUnderGravity)
    {
        // A dynamic body with centered collider under gravity alone must not
        // accumulate spurious rotation. This guards against integration drift and
        // ensures the quat -> euler -> quat round-trip in Scene::OnUpdateRuntime
        // does not introduce visible angular error.
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID id;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;

            TransformComponent tc;
            tc.Translation = { 0.0f, 10.0f, 0.0f };
            // Non-trivial initial rotation so any drift is visible.
            tc.Rotation = { 0.2f, 0.5f, 0.1f };

            world.CreateBody(id, rb, tc, { ColliderData{} });
        }

        const glm::quat initialRotation = world.GetRotation(id);

        for (int i = 0; i < 120; ++i)
            world.Step(1.0f / 60.0f);

        const glm::quat finalRotation = world.GetRotation(id);

        // Quaternions q and -q represent the same orientation; compare the dot
        // product magnitude so sign differences don't cause false failures.
        const float dot = glm::abs(glm::dot(initialRotation, finalRotation));
        CHECK_CLOSE(1.0f, dot, 0.001f);
    }

    TEST(PhysicsWorld_MultipleColliders_BodyFallsUnderGravity)
    {
        // A single body carrying both a box and a sphere collider must still
        // respond to gravity and produce a non-zero velocity after stepping.
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID id;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;

            TransformComponent tc;
            tc.Translation = { 0.0f, 5.0f, 0.0f };

            ColliderData boxCollider;
            boxCollider.Shape = ColliderShape::Box;
            boxCollider.HalfExtents = { 0.5f, 0.5f, 0.5f };

            ColliderData sphereCollider;
            sphereCollider.Shape = ColliderShape::Sphere;
            sphereCollider.Radius = 0.3f;
            sphereCollider.Offset = { 1.0f, 0.0f, 0.0f };

            world.CreateBody(id, rb, tc, { boxCollider, sphereCollider });
        }

        for (int i = 0; i < 30; ++i)
            world.Step(1.0f / 60.0f);

        CHECK(world.GetPosition(id).y < 5.0f - 0.1f);
        CHECK(world.GetLinearVelocity(id).y < -1.0f);
    }

    TEST(PhysicsWorld_KinematicBody_MovesWithSetVelocity)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f }); // gravity-free

        const UUID id;
        MakeBody(world, id, RigidBodyComponent::BodyType::Kinematic, { 0.0f, 0.0f, 0.0f });

        world.SetLinearVelocity(id, { 2.0f, 0.0f, 0.0f });

        const float startX = world.GetPosition(id).x;
        for (int i = 0; i < 30; ++i)
            world.Step(1.0f / 60.0f);

        CHECK(world.GetPosition(id).x > startX + 0.5f);
        CHECK_CLOSE(2.0f, world.GetLinearVelocity(id).x, 0.01f);
    }

    TEST(PhysicsWorld_GravityScale_AffectsFallSpeed)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID lightId;
        const UUID heavyId;

        {
            RigidBodyComponent rbLight;
            rbLight.Type = RigidBodyComponent::BodyType::Dynamic;
            rbLight.GravityScale = 0.5f;

            RigidBodyComponent rbHeavy;
            rbHeavy.Type = RigidBodyComponent::BodyType::Dynamic;
            rbHeavy.GravityScale = 2.0f;

            TransformComponent tc;
            tc.Translation = { -1.0f, 10.0f, 0.0f };
            world.CreateBody(lightId, rbLight, tc, { ColliderData{} });

            tc.Translation = { 1.0f, 10.0f, 0.0f };
            world.CreateBody(heavyId, rbHeavy, tc, { ColliderData{} });
        }

        for (int i = 0; i < 30; ++i)
            world.Step(1.0f / 60.0f);

        const float lightY = world.GetPosition(lightId).y;
        const float heavyY = world.GetPosition(heavyId).y;

        // Both should have fallen, but the heavy body (gravity scale 2.0) must
        // have fallen further than the light body (gravity scale 0.5).
        CHECK(lightY < 10.0f - 0.1f);
        CHECK(heavyY < lightY - 0.1f);
    }

    TEST(PhysicsWorld_LinearDamping_StopsBodyEventually)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f }); // gravity-free

        const UUID id;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;
            rb.LinearDamping = 5.0f; // high damping so velocity decays quickly

            TransformComponent tc;
            tc.Translation = { 0.0f, 0.0f, 0.0f };

            world.CreateBody(id, rb, tc, { ColliderData{} });
        }

        world.SetLinearVelocity(id, { 10.0f, 0.0f, 0.0f });

        // Step enough frames that damping should kill almost all velocity.
        for (int i = 0; i < 120; ++i)
            world.Step(1.0f / 60.0f);

        CHECK_CLOSE(0.0f, world.GetLinearVelocity(id).x, 0.01f);
        // The body should have moved but settled at a final position.
        CHECK(world.GetPosition(id).x > 0.0f);
    }

    TEST(Scene_RuntimeDynamicBody_FallsUnderGravity)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Falling");
        entity.GetComponent<TransformComponent>().Translation = { 0.0f, 10.0f, 0.0f };
        entity.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;
        entity.AddComponent<BoxColliderComponent>();

        scene->OnRuntimeStart();
        StepScene(scene, 30);
        const float y = entity.GetComponent<TransformComponent>().Translation.y;
        scene->OnRuntimeStop();

        CHECK(y < 10.0f - 0.1f);
    }

    TEST(Scene_RuntimeStaticBody_StaysPut)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Ground");
        entity.GetComponent<TransformComponent>().Translation = { 0.0f, 5.0f, 0.0f };
        entity.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Static;
        entity.AddComponent<BoxColliderComponent>();

        scene->OnRuntimeStart();
        StepScene(scene, 30);
        const float y = entity.GetComponent<TransformComponent>().Translation.y;
        scene->OnRuntimeStop();

        CHECK_CLOSE(5.0f, y, 0.001f);
    }

    TEST(Scene_RuntimeBodyWithoutCollider_StillFalls)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("NoCollider");
        entity.GetComponent<TransformComponent>().Translation = { 0.0f, 10.0f, 0.0f };
        entity.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;

        scene->OnRuntimeStart();
        StepScene(scene, 30);
        const float y = entity.GetComponent<TransformComponent>().Translation.y;
        scene->OnRuntimeStop();

        CHECK(y < 10.0f - 0.1f);
    }

    TEST(SceneSerializer_PhysicsComponents_SurviveSaveAndLoad)
    {
        const UUID id;
        const Ref<Scene> scene = CreateRef<Scene>();
        {
            Entity entity = scene->CreateEntityWithUUID(id, "Body");

            auto& rb = entity.AddComponent<RigidBodyComponent>();
            rb.Type = RigidBodyComponent::BodyType::Dynamic;
            rb.GravityScale = 2.0f;
            rb.LinearDamping = 0.3f;
            rb.AngularDamping = 0.4f;

            auto& box = entity.AddComponent<BoxColliderComponent>();
            box.HalfSize = { 1.0f, 2.0f, 3.0f };
            box.Offset = { 0.1f, 0.2f, 0.3f };
            box.Density = 1.5f; box.Friction = 0.25f; box.Restitution = 0.6f;

            auto& sphere = entity.AddComponent<SphereColliderComponent>();
            sphere.Radius = 0.75f;
            sphere.Offset = { 0.4f, 0.5f, 0.6f };
            sphere.Density = 2.5f; sphere.Friction = 0.35f; sphere.Restitution = 0.7f;

            auto& capsule = entity.AddComponent<CapsuleColliderComponent>();
            capsule.Radius = 0.9f; capsule.Height = 1.8f;
            capsule.Offset = { 0.7f, 0.8f, 0.9f };
            capsule.Density = 3.5f; capsule.Friction = 0.45f; capsule.Restitution = 0.8f;
        }

        const Testing::TempDir dir;
        const auto path = dir.File("physics.epscene");
        REQUIRE CHECK(SceneSerializer(scene).Serialize(path));

        const Ref<Scene> loaded = CreateRef<Scene>();
        REQUIRE CHECK(SceneSerializer(loaded).Deserialize(path));

        Entity entity = loaded->GetEntityByUUID(id);
        REQUIRE CHECK(static_cast<bool>(entity));

        const auto& rb = entity.GetComponent<RigidBodyComponent>();
        CHECK(rb.Type == RigidBodyComponent::BodyType::Dynamic);
        CHECK_CLOSE(2.0f, rb.GravityScale, 1e-5f);
        CHECK_CLOSE(0.3f, rb.LinearDamping, 1e-5f);
        CHECK_CLOSE(0.4f, rb.AngularDamping, 1e-5f);

        const auto& box = entity.GetComponent<BoxColliderComponent>();
        CHECK_VEC3_CLOSE(glm::vec3(1.0f, 2.0f, 3.0f), box.HalfSize, 1e-5f);
        CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), box.Offset, 1e-5f);
        CHECK_CLOSE(1.5f, box.Density, 1e-5f);
        CHECK_CLOSE(0.25f, box.Friction, 1e-5f);
        CHECK_CLOSE(0.6f, box.Restitution, 1e-5f);

        const auto& sphere = entity.GetComponent<SphereColliderComponent>();
        CHECK_CLOSE(0.75f, sphere.Radius, 1e-5f);
        CHECK_VEC3_CLOSE(glm::vec3(0.4f, 0.5f, 0.6f), sphere.Offset, 1e-5f);
        CHECK_CLOSE(2.5f, sphere.Density, 1e-5f);
        CHECK_CLOSE(0.35f, sphere.Friction, 1e-5f);
        CHECK_CLOSE(0.7f, sphere.Restitution, 1e-5f);

        const auto& capsule = entity.GetComponent<CapsuleColliderComponent>();
        CHECK_CLOSE(0.9f, capsule.Radius, 1e-5f);
        CHECK_CLOSE(1.8f, capsule.Height, 1e-5f);
        CHECK_VEC3_CLOSE(glm::vec3(0.7f, 0.8f, 0.9f), capsule.Offset, 1e-5f);
        CHECK_CLOSE(3.5f, capsule.Density, 1e-5f);
        CHECK_CLOSE(0.45f, capsule.Friction, 1e-5f);
        CHECK_CLOSE(0.8f, capsule.Restitution, 1e-5f);
    }
}
