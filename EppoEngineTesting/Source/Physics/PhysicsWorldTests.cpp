#include "Support/EppoTest.h"

#include "Core/UUID.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Components.h"
#include "Support/GlmCheck.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

using namespace Eppo;

// PhysicsWorld wraps Box3D directly (no scene/ECS, no GPU), so this is a cheap
// 'unit' suite: build a world, create bodies, step, assert poses/velocities.
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
    }

    TEST(DynamicBodyFallsUnderGravity)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID id;
        MakeDynamic(world, id, { 0.0f, 10.0f, 0.0f });

        const float startY = world.GetPosition(id).y;
        for (int i = 0; i < 60; ++i)
            world.Step(1.0f / 60.0f);

        CHECK(world.GetPosition(id).y < startY - 0.1f);
    }

    TEST(ApplyLinearImpulseAddsVelocity)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f }); // gravity-free so only the impulse acts

        const UUID id;
        MakeDynamic(world, id, glm::vec3(0.0f));

        world.ApplyLinearImpulse(id, { 5.0f, 0.0f, 0.0f });
        world.Step(1.0f / 60.0f);

        CHECK(world.GetLinearVelocity(id).x > 0.0f);
    }

    TEST(SetLinearVelocityRoundTrips)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f });

        const UUID id;
        MakeDynamic(world, id, glm::vec3(0.0f));

        world.SetLinearVelocity(id, { 0.0f, 0.0f, 3.0f });
        CHECK_CLOSE(3.0f, world.GetLinearVelocity(id).z, 0.001f);
    }

    TEST(StaticBodyDoesNotMove)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID id;
        MakeBody(world, id, RigidBodyComponent::BodyType::Static, { 0.0f, 5.0f, 0.0f });

        for (int i = 0; i < 30; ++i)
            world.Step(1.0f / 60.0f);

        CHECK_CLOSE(5.0f, world.GetPosition(id).y, 0.001f);
    }

    TEST(UnknownEntityAccessorsAreSafe)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID missing;
        CHECK(!world.HasBody(missing));
        // Must not crash; velocity of an unknown body reads as zero.
        world.ApplyLinearImpulse(missing, { 1.0f, 0.0f, 0.0f });
        CHECK_CLOSE(0.0f, world.GetLinearVelocity(missing).x, 0.001f);
    }

    TEST(SphereColliderFallsUnderGravity)
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

    TEST(CapsuleColliderFallsUnderGravity)
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

    TEST(RotationPreservedUnderGravity)
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

    TEST(MultipleCollidersOnOneBody)
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

    TEST(KinematicBodyMovesWithSetVelocity)
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

    TEST(GravityScaleAffectsFallSpeed)
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

    TEST(DynamicBodyWithDampingStopsEventually)
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
}
