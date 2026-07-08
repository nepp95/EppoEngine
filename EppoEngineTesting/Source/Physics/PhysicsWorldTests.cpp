#include "Support/EppoTest.h"

#include "Core/UUID.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Components.h"

using namespace Eppo;

// PhysicsWorld wraps Box3D directly (no scene/ECS, no GPU), so these run as a
// cheap 'unit' suite: build a world, create bodies from component data, step, and
// assert the simulated poses/velocities.
SUITE(Physics)
{
    namespace
    {
        auto MakeDynamic(PhysicsWorld& world, const UUID id, const glm::vec3& position) -> void
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;

            TransformComponent tc;
            tc.Translation = position;

            const BoxColliderComponent box; // defaults: half-extents 0.5, density 1
            world.CreateBody(id, rb, tc, &box, nullptr, nullptr);
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
        RigidBodyComponent rb; // default: Static
        TransformComponent tc;
        tc.Translation = { 0.0f, 5.0f, 0.0f };
        const BoxColliderComponent box;
        world.CreateBody(id, rb, tc, &box, nullptr, nullptr);

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
}
