#include "Support/EppoTest.h"

#include "Core/UUID.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Components.h"

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
}
