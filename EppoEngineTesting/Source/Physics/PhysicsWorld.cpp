#include "Support/EppoTest.h"
#include "Support/GlmCheck.h"
#include "Support/TempDir.h"

#include "Physics/PhysicsWorld.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace Eppo;

// Box3D world in isolation, plus scene-runtime physics and component
// serialization further down — all headless ('unit' suite).
SUITE(Physics)
{
    namespace
    {
        constexpr glm::quat s_Identity = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

        auto MakeBody(PhysicsWorld& world, const UUID id, RigidBodyComponent::BodyType type, const glm::vec3& position) -> void
        {
            RigidBodyComponent rb;
            rb.Type = type;

            world.CreateBody(id, rb, position, s_Identity, { ColliderData{} }); // default ColliderData: unit-ish box, density 1
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

            ColliderData collider;
            collider.Shape = ColliderShape::Sphere;
            collider.Radius = 0.5f;

            world.CreateBody(id, rb, { 0.0f, 10.0f, 0.0f }, s_Identity, { collider });
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

            ColliderData collider;
            collider.Shape = ColliderShape::Capsule;
            collider.Radius = 0.5f;
            collider.Height = 1.0f;

            world.CreateBody(id, rb, { 0.0f, 10.0f, 0.0f }, s_Identity, { collider });
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

            // Non-trivial initial rotation so any drift is visible.
            world.CreateBody(id, rb, { 0.0f, 10.0f, 0.0f }, glm::quat(glm::vec3(0.2f, 0.5f, 0.1f)), { ColliderData{} });
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

            ColliderData boxCollider;
            boxCollider.Shape = ColliderShape::Box;
            boxCollider.HalfExtents = { 0.5f, 0.5f, 0.5f };

            ColliderData sphereCollider;
            sphereCollider.Shape = ColliderShape::Sphere;
            sphereCollider.Radius = 0.3f;
            sphereCollider.Offset = { 1.0f, 0.0f, 0.0f };

            world.CreateBody(id, rb, { 0.0f, 5.0f, 0.0f }, s_Identity, { boxCollider, sphereCollider });
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

            world.CreateBody(lightId, rbLight, { -1.0f, 10.0f, 0.0f }, s_Identity, { ColliderData{} });
            world.CreateBody(heavyId, rbHeavy, { 1.0f, 10.0f, 0.0f }, s_Identity, { ColliderData{} });
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

            world.CreateBody(id, rb, glm::vec3(0.0f), s_Identity, { ColliderData{} });
        }

        world.SetLinearVelocity(id, { 10.0f, 0.0f, 0.0f });

        // Step enough frames that damping should kill almost all velocity.
        for (int i = 0; i < 120; ++i)
            world.Step(1.0f / 60.0f);

        CHECK_CLOSE(0.0f, world.GetLinearVelocity(id).x, 0.01f);
        // The body should have moved but settled at a final position.
        CHECK(world.GetPosition(id).x > 0.0f);
    }

    TEST(PhysicsWorld_LockedLinearY_BodyDoesNotFallUnderGravity)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID id;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;
            rb.LockLinearY = true;

            world.CreateBody(id, rb, { 0.0f, 10.0f, 0.0f }, s_Identity, { ColliderData{} });
        }

        for (int i = 0; i < 60; ++i)
            world.Step(1.0f / 60.0f);

        CHECK_CLOSE(10.0f, world.GetPosition(id).y, 1e-3f);

        // The lock is per-axis: X/Z must still translate freely.
        world.ApplyLinearImpulse(id, { 2.0f, 0.0f, 0.0f });
        for (int i = 0; i < 30; ++i)
            world.Step(1.0f / 60.0f);

        CHECK(world.GetPosition(id).x > 0.1f);
        CHECK_CLOSE(10.0f, world.GetPosition(id).y, 1e-3f);
    }

    TEST(PhysicsWorld_LockedAngularAxes_BodyDoesNotTipOffLedge)
    {
        // A body resting with its center of mass past the slab edge has a contact
        // torque that tips an unlocked body; locking all angular axes must keep it level.
        const auto restOnLedge = [](const bool lockAngular) -> glm::quat
        {
            PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

            const UUID slabId;
            {
                RigidBodyComponent rb; // static

                ColliderData slab;
                slab.HalfExtents = { 2.0f, 0.1f, 2.0f };

                world.CreateBody(slabId, rb, glm::vec3(0.0f), s_Identity, { slab });
            }

            const UUID bodyId;
            {
                RigidBodyComponent rb;
                rb.Type = RigidBodyComponent::BodyType::Dynamic;
                rb.LockAngularX = lockAngular;
                rb.LockAngularY = lockAngular;
                rb.LockAngularZ = lockAngular;

                ColliderData box;
                box.HalfExtents = { 0.25f, 0.75f, 0.25f };

                // Slab spans x in [-2, 2]; the tall body's center at x = 2.1 is
                // past the edge, so gravity tips an unlocked body over it.
                world.CreateBody(bodyId, rb, { 2.1f, 0.85f, 0.0f }, s_Identity, { box });
            }

            for (int i = 0; i < 120; ++i)
                world.Step(1.0f / 60.0f);

            return world.GetRotation(bodyId);
        };

        const glm::quat unlocked = restOnLedge(false);
        CHECK(glm::abs(unlocked.w) < 0.99f);

        const glm::quat locked = restOnLedge(true);
        CHECK_CLOSE(1.0f, glm::abs(locked.w), 0.001f);
    }

    TEST(PhysicsWorld_CreateBody_UsesWorldPose)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f });

        const UUID id;
        RigidBodyComponent rb;

        const glm::quat rotation(glm::vec3(0.0f, 0.0f, glm::half_pi<float>()));
        world.CreateBody(id, rb, { 1.0f, 2.0f, 3.0f }, rotation, { ColliderData{} });

        CHECK_VEC3_CLOSE(glm::vec3(1.0f, 2.0f, 3.0f), world.GetPosition(id), 1e-5f);
        CHECK_CLOSE(1.0f, glm::abs(glm::dot(rotation, world.GetRotation(id))), 1e-5f);
    }

    TEST(PhysicsWorld_GetShapeCount_CountsAttachedColliders)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f });

        const UUID id;
        RigidBodyComponent rb;
        rb.Type = RigidBodyComponent::BodyType::Dynamic;

        ColliderData box;
        ColliderData sphere;
        sphere.Shape = ColliderShape::Sphere;
        sphere.Offset = { 1.0f, 0.0f, 0.0f };

        world.CreateBody(id, rb, glm::vec3(0.0f), s_Identity, { box, sphere });

        CHECK_EQUAL(2, world.GetShapeCount(id));
        CHECK_EQUAL(0, world.GetShapeCount(UUID()));
    }

    TEST(PhysicsWorld_RotatedBoxCollider_CollidesAtRotatedPose)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f }); // gravity-free: only the launched ball moves

        const UUID slabId;
        {
            RigidBodyComponent rb; // static

            // A tall thin wall rotated 90 degrees about Z becomes a flat slab
            // spanning x in [-2, 2]. Without the rotation it stays a wall at
            // x ~ 0 and the ball at x = 1.5 drops straight past it.
            ColliderData box;
            box.HalfExtents = { 0.05f, 2.0f, 0.5f };
            box.Rotation = glm::quat(glm::vec3(0.0f, 0.0f, glm::half_pi<float>()));

            world.CreateBody(slabId, rb, glm::vec3(0.0f), s_Identity, { box });
        }

        const UUID ballId;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;

            ColliderData sphere;
            sphere.Shape = ColliderShape::Sphere;

            world.CreateBody(ballId, rb, { 1.5f, 3.0f, 0.0f }, s_Identity, { sphere });
        }

        world.SetLinearVelocity(ballId, { 0.0f, -5.0f, 0.0f });
        for (int i = 0; i < 120; ++i)
            world.Step(1.0f / 60.0f);

        CHECK(world.GetPosition(ballId).y > 0.3f);
    }

    TEST(PhysicsWorld_RotatedCapsuleCollider_CollidesAtRotatedPose)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f });

        const UUID capsuleId;
        {
            RigidBodyComponent rb; // static

            // Rotated 90 degrees about Z the capsule axis lies along X, covering
            // |x| <= 1 plus end caps. Unrotated it only spans |x| <= 0.5, so the
            // ball at x = 0.9 would fall past it.
            ColliderData capsule;
            capsule.Shape = ColliderShape::Capsule;
            capsule.Height = 2.0f;
            capsule.Rotation = glm::quat(glm::vec3(0.0f, 0.0f, glm::half_pi<float>()));

            world.CreateBody(capsuleId, rb, glm::vec3(0.0f), s_Identity, { capsule });
        }

        const UUID ballId;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;

            ColliderData sphere;
            sphere.Shape = ColliderShape::Sphere;

            world.CreateBody(ballId, rb, { 0.9f, 3.0f, 0.0f }, s_Identity, { sphere });
        }

        world.SetLinearVelocity(ballId, { 0.0f, -5.0f, 0.0f });
        for (int i = 0; i < 120; ++i)
            world.Step(1.0f / 60.0f);

        CHECK(world.GetPosition(ballId).y > 0.5f);
    }

    TEST(PhysicsWorld_RotatedSphereCollider_KeepsOffsetCenter)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f });

        const UUID blockerId;
        {
            RigidBodyComponent rb; // static

            // Offset is the shape's final body-local translation; the shape
            // rotation must not re-rotate it (that would move the center to
            // (0, 2, 0) and let the ball pass).
            ColliderData sphere;
            sphere.Shape = ColliderShape::Sphere;
            sphere.Offset = { 2.0f, 0.0f, 0.0f };
            sphere.Rotation = glm::quat(glm::vec3(0.0f, 0.0f, glm::half_pi<float>()));

            world.CreateBody(blockerId, rb, glm::vec3(0.0f), s_Identity, { sphere });
        }

        const UUID ballId;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;

            ColliderData sphere;
            sphere.Shape = ColliderShape::Sphere;

            world.CreateBody(ballId, rb, { 5.0f, 0.0f, 0.0f }, s_Identity, { sphere });
        }

        world.SetLinearVelocity(ballId, { -5.0f, 0.0f, 0.0f });
        for (int i = 0; i < 120; ++i)
            world.Step(1.0f / 60.0f);

        CHECK(world.GetPosition(ballId).x > 1.5f);
    }

    TEST(PhysicsWorld_ZeroHeightCapsule_CollidesAsSphere)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f });

        const UUID capsuleId;
        {
            RigidBodyComponent rb; // static

            // Height 0 (e.g. an authored 0 or a zeroed scale) collapses both
            // hemisphere centers; the shape must degrade to a sphere of the
            // capsule radius rather than a broken zero-length segment.
            ColliderData capsule;
            capsule.Shape = ColliderShape::Capsule;
            capsule.Height = 0.0f;

            world.CreateBody(capsuleId, rb, glm::vec3(0.0f), s_Identity, { capsule });
        }

        const UUID ballId;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;

            ColliderData sphere;
            sphere.Shape = ColliderShape::Sphere;

            world.CreateBody(ballId, rb, { 3.0f, 0.0f, 0.0f }, s_Identity, { sphere });
        }

        world.SetLinearVelocity(ballId, { -5.0f, 0.0f, 0.0f });
        for (int i = 0; i < 120; ++i)
            world.Step(1.0f / 60.0f);

        // Radii 0.5 + 0.5: the ball must stop near x = 1, not sail through to x = -7.
        CHECK(world.GetPosition(ballId).x > 0.5f);
    }

    TEST(PhysicsWorld_CylinderCollider_BlocksSphere)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f });

        const UUID cylinderId;
        {
            RigidBodyComponent rb;
            ColliderData cylinder;
            cylinder.Shape = ColliderShape::Cylinder;
            cylinder.Radius = 1.0f;
            cylinder.Height = 2.0f;
            world.CreateBody(cylinderId, rb, glm::vec3(0.0f), s_Identity, { cylinder });
        }

        const UUID ballId;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;
            ColliderData sphere;
            sphere.Shape = ColliderShape::Sphere;
            world.CreateBody(ballId, rb, { 3.0f, 0.0f, 0.0f }, s_Identity, { sphere });
        }

        world.SetLinearVelocity(ballId, { -5.0f, 0.0f, 0.0f });
        for (int i = 0; i < 120; ++i)
            world.Step(1.0f / 60.0f);

        CHECK(world.GetPosition(ballId).x > 0.5f);

        const UUID topBallId;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;
            ColliderData sphere;
            sphere.Shape = ColliderShape::Sphere;
            world.CreateBody(topBallId, rb, { 0.0f, 3.0f, 0.0f }, s_Identity, { sphere });
        }

        world.SetLinearVelocity(topBallId, { 0.0f, -5.0f, 0.0f });
        for (int i = 0; i < 120; ++i)
            world.Step(1.0f / 60.0f);

        CHECK_CLOSE(1.5f, world.GetPosition(topBallId).y, 0.1f);
    }

    TEST(PhysicsWorld_DynamicCylinder_RestsOnFloorAtFullHeight)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID floorId;
        {
            RigidBodyComponent rb;
            ColliderData floor;
            floor.HalfExtents = { 5.0f, 0.5f, 5.0f };
            world.CreateBody(floorId, rb, glm::vec3(0.0f), s_Identity, { floor });
        }

        const UUID cylinderId;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;
            ColliderData cylinder;
            cylinder.Shape = ColliderShape::Cylinder;
            cylinder.Radius = 1.0f;
            cylinder.Height = 2.0f;
            world.CreateBody(cylinderId, rb, { 0.0f, 4.0f, 0.0f }, s_Identity, { cylinder });
        }

        for (int i = 0; i < 240; ++i)
            world.Step(1.0f / 60.0f);

        CHECK_CLOSE(1.5f, world.GetPosition(cylinderId).y, 0.1f);
    }

    TEST(PhysicsWorld_ZeroExtentBoxCollider_StillBlocks)
    {
        PhysicsWorld world({ 0.0f, 0.0f, 0.0f });

        const UUID plateId;
        {
            RigidBodyComponent rb; // static

            // A zeroed scale axis produces a zero half extent; the plate must
            // still block instead of producing a degenerate hull.
            ColliderData box;
            box.HalfExtents = { 2.0f, 0.0f, 2.0f };

            world.CreateBody(plateId, rb, glm::vec3(0.0f), s_Identity, { box });
        }

        const UUID ballId;
        {
            RigidBodyComponent rb;
            rb.Type = RigidBodyComponent::BodyType::Dynamic;

            ColliderData sphere;
            sphere.Shape = ColliderShape::Sphere;

            world.CreateBody(ballId, rb, { 0.0f, 3.0f, 0.0f }, s_Identity, { sphere });
        }

        world.SetLinearVelocity(ballId, { 0.0f, -2.0f, 0.0f });
        for (int i = 0; i < 120; ++i)
            world.Step(1.0f / 60.0f);

        CHECK(world.GetPosition(ballId).y > 0.0f);
    }

    TEST(PhysicsWorld_CastRay_HitsStaticBoxBelow)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID floorId;
        MakeBody(world, floorId, RigidBodyComponent::BodyType::Static, glm::vec3(0.0f)); // unit box, top at y=0.5
        world.Step(1.0f / 60.0f);

        const RayHit hit = world.CastRay({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f);
        REQUIRE CHECK(hit.Hit);
        CHECK_CLOSE(0.5f, hit.Point.y, 0.05f);
        CHECK_CLOSE(1.0f, hit.Normal.y, 0.05f);
        CHECK_CLOSE(4.5f, hit.Distance, 0.1f);
        CHECK_EQUAL(static_cast<uint64_t>(floorId), static_cast<uint64_t>(hit.EntityId));
    }

    TEST(PhysicsWorld_CastRay_MissesWhenOutOfRange)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID floorId;
        MakeBody(world, floorId, RigidBodyComponent::BodyType::Static, glm::vec3(0.0f));
        world.Step(1.0f / 60.0f);

        const RayHit hit = world.CastRay({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 1.0f);
        CHECK(!hit.Hit);
    }

    TEST(PhysicsWorld_CastRay_IgnoresInitialOverlap)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID id;
        MakeBody(world, id, RigidBodyComponent::BodyType::Static, glm::vec3(0.0f));
        world.Step(1.0f / 60.0f);

        // A ray whose origin lies inside a convex shape does not register that shape.
        const RayHit hit = world.CastRay(glm::vec3(0.0f), { 0.0f, -1.0f, 0.0f }, 10.0f);
        CHECK(!hit.Hit);
    }

    TEST(PhysicsWorld_OverlapsSphere_TrueInsideRadius_FalseOutside)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID id;
        MakeBody(world, id, RigidBodyComponent::BodyType::Static, glm::vec3(0.0f));
        world.Step(1.0f / 60.0f);

        CHECK(world.OverlapsSphere(id, { 0.0f, 0.0f, 0.0f }, 2.0f));
        CHECK(!world.OverlapsSphere(id, { 10.0f, 0.0f, 0.0f }, 2.0f));
    }

    TEST(PhysicsWorld_OverlapsSphere_MatchesOnlyTargetBody)
    {
        PhysicsWorld world({ 0.0f, -9.81f, 0.0f });

        const UUID a, b;
        MakeBody(world, a, RigidBodyComponent::BodyType::Static, glm::vec3(0.0f));
        MakeBody(world, b, RigidBodyComponent::BodyType::Static, glm::vec3(0.0f));
        world.Step(1.0f / 60.0f);

        const UUID missing;
        CHECK(world.OverlapsSphere(a, { 0.0f, 0.0f, 0.0f }, 2.0f));
        CHECK(world.OverlapsSphere(b, { 0.0f, 0.0f, 0.0f }, 2.0f));
        CHECK(!world.OverlapsSphere(missing, { 0.0f, 0.0f, 0.0f }, 2.0f));
        CHECK(!world.OverlapsSphere(a, { 10.0f, 0.0f, 0.0f }, 2.0f));
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

    TEST(Scene_RuntimeScaledBoxCollider_UsesEntityScale)
    {
        const Ref<Scene> scene = CreateRef<Scene>();

        Entity ground = scene->CreateEntity("Scaled ground");
        ground.GetComponent<TransformComponent>().Scale = { 1.0f, 4.0f, 1.0f };
        ground.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Static;
        ground.AddComponent<BoxColliderComponent>();

        Entity falling = scene->CreateEntity("Falling sphere");
        falling.GetComponent<TransformComponent>().Translation = { 0.0f, 4.0f, 0.0f };
        falling.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;
        falling.AddComponent<SphereColliderComponent>();

        scene->OnRuntimeStart();
        StepScene(scene, 120);
        const float y = falling.GetComponent<TransformComponent>().Translation.y;
        scene->OnRuntimeStop();

        CHECK(y > 2.0f);
    }

    TEST(Scene_RuntimeBodyWithoutCollider_StillFalls)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("NoCollider");
        entity.GetComponent<TransformComponent>().Translation = { 0.0f, 10.0f, 0.0f };
        entity.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;

        scene->OnRuntimeStart();
        const Ref<PhysicsWorld> physics = scene->GetPhysicsWorld();
        CHECK(physics->HasBody(entity.GetUUID()));
        CHECK_EQUAL(0, physics->GetShapeCount(entity.GetUUID()));
        CHECK_EQUAL(1, scene->GetColliderlessRigidBodies().size());
        CHECK_EQUAL("NoCollider", scene->GetColliderlessRigidBodies().front());
        StepScene(scene, 30);
        const float y = entity.GetComponent<TransformComponent>().Translation.y;
        scene->OnRuntimeStop();

        CHECK(y < 10.0f - 0.1f);
    }

    TEST(Scene_FitBoxColliderToMesh_UsesPrimitiveBounds)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Cube");
        entity.AddComponent<MeshComponent>(static_cast<uint64_t>(MeshPrimitiveType::Cube));

        auto& collider = entity.AddComponent<BoxColliderComponent>();
        scene->FitColliderToMesh(entity, collider);

        CHECK_VEC3_CLOSE(glm::vec3(0.0f), collider.Offset, 0.001f);
        CHECK_VEC3_CLOSE(glm::vec3(1.0f), collider.HalfSize, 0.001f);
    }

    TEST(Scene_FitSphereColliderToMesh_UsesPrimitiveBounds)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Cube");
        entity.AddComponent<MeshComponent>(static_cast<uint64_t>(MeshPrimitiveType::Cube));

        auto& collider = entity.AddComponent<SphereColliderComponent>();
        scene->FitColliderToMesh(entity, collider);

        CHECK_VEC3_CLOSE(glm::vec3(0.0f), collider.Offset, 0.001f);
        CHECK_CLOSE(1.0f, collider.Radius, 0.001f);
    }

    TEST(Scene_FitCapsuleColliderToMesh_UsesPrimitiveBounds)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Capsule");
        entity.AddComponent<MeshComponent>(static_cast<uint64_t>(MeshPrimitiveType::Capsule));

        auto& collider = entity.AddComponent<CapsuleColliderComponent>();
        scene->FitColliderToMesh(entity, collider);

        CHECK_VEC3_CLOSE(glm::vec3(0.0f), collider.Offset, 0.001f);
        CHECK_CLOSE(1.0f, collider.Radius, 0.001f);
        CHECK_CLOSE(2.0f, collider.Height, 0.001f);
    }

    TEST(Scene_FitCylinderColliderToMesh_UsesPrimitiveBounds)
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Cylinder");
        entity.AddComponent<MeshComponent>(static_cast<uint64_t>(MeshPrimitiveType::Cylinder));

        auto& collider = entity.AddComponent<CylinderColliderComponent>();
        scene->FitColliderToMesh(entity, collider);

        CHECK_VEC3_CLOSE(glm::vec3(0.0f), collider.Offset, 0.001f);
        CHECK_CLOSE(1.0f, collider.Radius, 0.001f);
        CHECK_CLOSE(2.0f, collider.Height, 0.001f);
    }

    TEST(Scene_RuntimeChildCollider_BecomesShapeOnRootBody)
    {
        const Ref<Scene> scene = CreateRef<Scene>();

        Entity root = scene->CreateEntity("Root");
        root.GetComponent<TransformComponent>().Translation = { 0.0f, 10.0f, 0.0f };
        root.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;
        root.AddComponent<BoxColliderComponent>();

        Entity child = scene->CreateEntity("Child");
        scene->SetParent(child, root);
        child.GetComponent<TransformComponent>().Translation = { 2.0f, 0.0f, 0.0f };
        child.AddComponent<BoxColliderComponent>();

        scene->OnRuntimeStart();
        const Ref<PhysicsWorld> physics = scene->GetPhysicsWorld();
        CHECK(physics->HasBody(root.GetUUID()));
        CHECK(!physics->HasBody(child.GetUUID()));
        CHECK_EQUAL(2, physics->GetShapeCount(root.GetUUID()));
        scene->OnRuntimeStop();
    }

    TEST(Scene_RuntimeMultipleChildBoxColliders_EachContributesShape)
    {
        const Ref<Scene> scene = CreateRef<Scene>();

        Entity root = scene->CreateEntity("Root");
        root.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;

        for (int i = 0; i < 3; ++i)
        {
            Entity child = scene->CreateEntity("Child");
            scene->SetParent(child, root);
            child.GetComponent<TransformComponent>().Translation = { static_cast<float>(i) * 2.0f, 0.0f, 0.0f };
            child.AddComponent<BoxColliderComponent>();
        }

        scene->OnRuntimeStart();
        // Exactly the three child shapes: no default fallback box may be added
        // once the gathered collider list is non-empty.
        CHECK_EQUAL(3, scene->GetPhysicsWorld()->GetShapeCount(root.GetUUID()));
        scene->OnRuntimeStop();
    }

    TEST(Scene_RuntimeChildColliderOffsets_CollideAtWorldPose)
    {
        const Ref<Scene> scene = CreateRef<Scene>();

        // Two narrow pads under x = -2 and x = 2 only; nothing under the root
        // center, so a body whose shapes ignore the child offsets falls through.
        for (const float x : { -2.0f, 2.0f })
        {
            Entity pad = scene->CreateEntity("Pad");
            pad.GetComponent<TransformComponent>().Translation = { x, -0.5f, 0.0f };
            pad.AddComponent<RigidBodyComponent>();
            pad.AddComponent<BoxColliderComponent>();
        }

        Entity root = scene->CreateEntity("Root");
        root.GetComponent<TransformComponent>().Translation = { 0.0f, 3.0f, 0.0f };
        root.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;

        for (const float x : { -2.0f, 2.0f })
        {
            Entity child = scene->CreateEntity("Child");
            scene->SetParent(child, root);
            child.GetComponent<TransformComponent>().Translation = { x, 0.0f, 0.0f };
            child.AddComponent<BoxColliderComponent>();
        }

        scene->OnRuntimeStart();
        StepScene(scene, 180);
        const float y = root.GetComponent<TransformComponent>().Translation.y;
        scene->OnRuntimeStop();

        // Child boxes rest on the pads: root settles around y = 0.5.
        CHECK(y > 0.2f);
    }

    TEST(Scene_RuntimeRotatedChildBoxCollider_CollidesAtRotatedPose)
    {
        const Ref<Scene> scene = CreateRef<Scene>();

        Entity root = scene->CreateEntity("Root");
        root.AddComponent<RigidBodyComponent>();

        // Tall thin wall rotated 90 degrees about Z becomes a slab spanning
        // x in [-2, 2]; unrotated it is a wall at x ~ 0 and the ball falls past.
        Entity child = scene->CreateEntity("Slab");
        scene->SetParent(child, root);
        child.GetComponent<TransformComponent>().Rotation = { 0.0f, 0.0f, glm::half_pi<float>() };
        child.AddComponent<BoxColliderComponent>().HalfSize = { 0.05f, 2.0f, 0.5f };

        Entity ball = scene->CreateEntity("Ball");
        ball.GetComponent<TransformComponent>().Translation = { 1.5f, 3.0f, 0.0f };
        ball.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;
        ball.AddComponent<SphereColliderComponent>();

        scene->OnRuntimeStart();
        StepScene(scene, 180);
        const float y = ball.GetComponent<TransformComponent>().Translation.y;
        scene->OnRuntimeStop();

        CHECK(y > 0.3f);
    }

    TEST(Scene_RuntimeNestedScaledChain_ScalesColliderShape)
    {
        const Ref<Scene> scene = CreateRef<Scene>();

        Entity root = scene->CreateEntity("Root");
        root.AddComponent<RigidBodyComponent>();

        // Parent everything while transforms are still identity: SetParent
        // preserves world poses, so the locals set afterwards mean exactly what
        // they say.
        Entity mid = scene->CreateEntity("Mid");
        scene->SetParent(mid, root);

        // World center x = 1 + 2 * 2 = 5, world half extent x = 1: the platform
        // spans x in [4, 6]. Without the chain's scale it spans [2.5, 3.5] and
        // the ball at x = 5.7 falls past.
        Entity leaf = scene->CreateEntity("Leaf");
        scene->SetParent(leaf, mid);
        leaf.AddComponent<BoxColliderComponent>();

        mid.GetComponent<TransformComponent>().Translation = { 1.0f, 0.0f, 0.0f };
        mid.GetComponent<TransformComponent>().Scale = { 2.0f, 1.0f, 1.0f };
        leaf.GetComponent<TransformComponent>().Translation = { 2.0f, 0.0f, 0.0f };

        Entity ball = scene->CreateEntity("Ball");
        ball.GetComponent<TransformComponent>().Translation = { 5.7f, 2.0f, 0.0f };
        ball.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;
        ball.AddComponent<SphereColliderComponent>();

        scene->OnRuntimeStart();
        StepScene(scene, 180);
        const float y = ball.GetComponent<TransformComponent>().Translation.y;
        scene->OnRuntimeStop();

        CHECK(y > 0.6f);
    }

    TEST(Scene_RuntimeMirroredScale_MirrorsColliderOffset)
    {
        const Ref<Scene> scene = CreateRef<Scene>();

        Entity platform = scene->CreateEntity("Mirrored platform");
        platform.GetComponent<TransformComponent>().Scale = { -1.0f, 1.0f, 1.0f };
        platform.AddComponent<RigidBodyComponent>();
        auto& collider = platform.AddComponent<BoxColliderComponent>();
        collider.Offset = { 2.0f, 0.0f, 0.0f };

        Entity ball = scene->CreateEntity("Ball");
        ball.GetComponent<TransformComponent>().Translation = { -2.0f, 2.0f, 0.0f };
        ball.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;
        ball.AddComponent<SphereColliderComponent>();

        scene->OnRuntimeStart();
        StepScene(scene, 180);
        const float y = ball.GetComponent<TransformComponent>().Translation.y;
        scene->OnRuntimeStop();

        CHECK(y > 0.6f);
    }

    TEST(Scene_RuntimeNestedRotatedChain_RotatesColliderOffset)
    {
        const Ref<Scene> scene = CreateRef<Scene>();

        Entity root = scene->CreateEntity("Root");
        root.AddComponent<RigidBodyComponent>();

        // Parent first, then set locals (SetParent preserves world poses).
        Entity mid = scene->CreateEntity("Mid");
        scene->SetParent(mid, root);

        // The mid rotation about Y carries the leaf offset (2, 0, 0) to world
        // (0, 0, -2); without it the platform sits at (2, 0, 0) and the ball
        // dropped at (0, 2, -2) falls past.
        Entity leaf = scene->CreateEntity("Leaf");
        scene->SetParent(leaf, mid);
        leaf.AddComponent<BoxColliderComponent>();

        mid.GetComponent<TransformComponent>().Rotation = { 0.0f, glm::half_pi<float>(), 0.0f };
        leaf.GetComponent<TransformComponent>().Translation = { 2.0f, 0.0f, 0.0f };

        Entity ball = scene->CreateEntity("Ball");
        ball.GetComponent<TransformComponent>().Translation = { 0.0f, 2.0f, -2.0f };
        ball.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;
        ball.AddComponent<SphereColliderComponent>();

        scene->OnRuntimeStart();
        StepScene(scene, 180);
        const float y = ball.GetComponent<TransformComponent>().Translation.y;
        scene->OnRuntimeStop();

        CHECK(y > 0.6f);
    }

    TEST(Scene_RuntimeNestedRigidBody_StartsOwnBodyAndBoundsGathering)
    {
        const Ref<Scene> scene = CreateRef<Scene>();

        Entity root = scene->CreateEntity("Root");
        root.GetComponent<TransformComponent>().Translation = { 3.0f, 0.0f, 0.0f };
        root.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;
        root.AddComponent<BoxColliderComponent>();

        Entity child = scene->CreateEntity("Child");
        scene->SetParent(child, root);
        child.GetComponent<TransformComponent>().Translation = { 0.0f, 4.0f, 0.0f };
        child.AddComponent<RigidBodyComponent>();
        child.AddComponent<BoxColliderComponent>();

        Entity grandchild = scene->CreateEntity("Grandchild");
        scene->SetParent(grandchild, child);
        grandchild.GetComponent<TransformComponent>().Translation = { 1.0f, 0.0f, 0.0f };
        grandchild.AddComponent<BoxColliderComponent>();

        scene->OnRuntimeStart();
        const Ref<PhysicsWorld> physics = scene->GetPhysicsWorld();

        CHECK(physics->HasBody(root.GetUUID()));
        CHECK(physics->HasBody(child.GetUUID()));
        // The nested rigid body claims its own subtree: the grandchild shape
        // belongs to the child body, not the root's.
        CHECK_EQUAL(1, physics->GetShapeCount(root.GetUUID()));
        CHECK_EQUAL(2, physics->GetShapeCount(child.GetUUID()));
        // The nested body starts at its world pose, not its local translation.
        CHECK_VEC3_CLOSE(glm::vec3(3.0f, 4.0f, 0.0f), physics->GetPosition(child.GetUUID()), 1e-4f);

        scene->OnRuntimeStop();
    }

    TEST(Scene_CopiedThenRun_GathersChildCollidersOntoRootBody)
    {
        // Mirrors the editor Play flow: the authoring scene is copied and the
        // copy is simulated, so the hierarchy must survive Scene::Copy.
        const Ref<Scene> authoring = CreateRef<Scene>();

        Entity vehicle = authoring->CreateEntity("Vehicle");
        vehicle.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;

        Entity chassis = authoring->CreateEntity("Chassis");
        authoring->SetParent(chassis, vehicle);
        chassis.AddComponent<BoxColliderComponent>();

        Entity sensor = authoring->CreateEntity("FrontSensor");
        authoring->SetParent(sensor, vehicle);
        sensor.GetComponent<TransformComponent>().Translation = { 0.0f, 0.0f, 2.0f };
        sensor.AddComponent<SphereColliderComponent>();

        const Ref<Scene> runtime = Scene::Copy(authoring);
        Entity runtimeVehicle = runtime->GetEntityByUUID(vehicle.GetUUID());

        runtime->OnRuntimeStart();
        const int shapeCount = runtime->GetPhysicsWorld()->GetShapeCount(runtimeVehicle.GetUUID());
        runtime->OnRuntimeStop();

        // Both descendant colliders must land on the single root body.
        CHECK_EQUAL(2, shapeCount);
    }

    TEST(Scene_RuntimeParentedDynamicRoot_SyncsWithoutDoubleParentTransform)
    {
        const Ref<Scene> scene = CreateRef<Scene>();

        Entity holder = scene->CreateEntity("Holder");

        // Local (1, 0, 0) under the rotated holder puts the body at world (10, 6, 0).
        // Parent first, then set locals (SetParent preserves world poses).
        Entity body = scene->CreateEntity("Body");
        scene->SetParent(body, holder);
        body.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;
        body.AddComponent<BoxColliderComponent>();

        holder.GetComponent<TransformComponent>().Translation = { 10.0f, 5.0f, 0.0f };
        holder.GetComponent<TransformComponent>().Rotation = { 0.0f, 0.0f, glm::half_pi<float>() };
        body.GetComponent<TransformComponent>().Translation = { 1.0f, 0.0f, 0.0f };
        body.GetComponent<TransformComponent>().Scale = { 2.0f, 2.0f, 2.0f };

        scene->OnRuntimeStart();
        CHECK_VEC3_CLOSE(glm::vec3(10.0f, 6.0f, 0.0f), scene->GetPhysicsWorld()->GetPosition(body.GetUUID()), 1e-4f);

        StepScene(scene, 30);
        const glm::vec3 worldPosition = glm::vec3(scene->GetWorldTransform(body)[3]);
        const glm::vec3 localScale = body.GetComponent<TransformComponent>().Scale;
        scene->OnRuntimeStop();

        // Free fall is straight down in world space; the holder transform must
        // not be applied a second time on top of the physics pose.
        CHECK_CLOSE(10.0f, worldPosition.x, 0.01f);
        CHECK(worldPosition.y < 6.0f - 0.1f);
        CHECK_CLOSE(0.0f, worldPosition.z, 0.01f);
        CHECK_VEC3_CLOSE(glm::vec3(2.0f), localScale, 1e-5f);
    }

    TEST(Scene_RuntimeChildBodyUnderMovingParent_KeepsWorldPose)
    {
        const Ref<Scene> scene = CreateRef<Scene>();

        // Parent created first: entt views visit newest-first, so the child body
        // is synced before its parent — the order that exposes a conversion
        // against the parent's previous-frame transform.
        Entity parent = scene->CreateEntity("Parent");
        parent.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Kinematic;
        parent.AddComponent<BoxColliderComponent>();

        Entity child = scene->CreateEntity("Child");
        scene->SetParent(child, parent);
        child.GetComponent<TransformComponent>().Translation = { 0.0f, 5.0f, 0.0f };
        child.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Kinematic;
        child.AddComponent<BoxColliderComponent>();

        scene->OnRuntimeStart();
        scene->GetPhysicsWorld()->SetLinearVelocity(parent.GetUUID(), { 10.0f, 0.0f, 0.0f });

        StepScene(scene, 30);
        const glm::vec3 parentPosition = parent.GetComponent<TransformComponent>().Translation;
        const glm::vec3 childWorld = glm::vec3(scene->GetWorldTransform(child)[3]);
        scene->OnRuntimeStop();

        CHECK(parentPosition.x > 4.0f);
        // The child's own body never moves, so its composed world pose must stay
        // put no matter how the parent body moved this frame.
        CHECK_CLOSE(0.0f, childWorld.x, 1e-3f);
        CHECK_CLOSE(5.0f, childWorld.y, 1e-3f);
    }

    TEST(Scene_RuntimeColliderWithoutRigidBodyAncestor_GetsNoBody)
    {
        const Ref<Scene> scene = CreateRef<Scene>();

        Entity orphan = scene->CreateEntity("Orphan");
        orphan.GetComponent<TransformComponent>().Translation = { 0.0f, 5.0f, 0.0f };
        orphan.AddComponent<BoxColliderComponent>();

        scene->OnRuntimeStart();
        CHECK(!scene->GetPhysicsWorld()->HasBody(orphan.GetUUID()));

        StepScene(scene, 10);
        // No body means physics never writes this transform.
        CHECK_CLOSE(5.0f, orphan.GetComponent<TransformComponent>().Translation.y, 1e-5f);
        scene->OnRuntimeStop();
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
            rb.LockLinearY = true;
            rb.LockAngularX = true;
            rb.LockAngularZ = true;

            auto& box = entity.AddComponent<BoxColliderComponent>();
            box.HalfSize = { 1.0f, 2.0f, 3.0f };
            box.Offset = { 0.1f, 0.2f, 0.3f };
            box.Density = 1.5f;
            box.Friction = 0.25f;
            box.Restitution = 0.6f;

            auto& sphere = entity.AddComponent<SphereColliderComponent>();
            sphere.Radius = 0.75f;
            sphere.Offset = { 0.4f, 0.5f, 0.6f };
            sphere.Density = 2.5f;
            sphere.Friction = 0.35f;
            sphere.Restitution = 0.7f;

            auto& capsule = entity.AddComponent<CapsuleColliderComponent>();
            capsule.Radius = 0.9f;
            capsule.Height = 1.8f;
            capsule.Offset = { 0.7f, 0.8f, 0.9f };
            capsule.Density = 3.5f;
            capsule.Friction = 0.45f;
            capsule.Restitution = 0.8f;

            auto& cylinder = entity.AddComponent<CylinderColliderComponent>();
            cylinder.Radius = 1.1f;
            cylinder.Height = 2.2f;
            cylinder.Offset = { 1.0f, 1.1f, 1.2f };
            cylinder.Density = 4.5f;
            cylinder.Friction = 0.55f;
            cylinder.Restitution = 0.9f;
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
        CHECK(!rb.LockLinearX);
        CHECK(rb.LockLinearY);
        CHECK(!rb.LockLinearZ);
        CHECK(rb.LockAngularX);
        CHECK(!rb.LockAngularY);
        CHECK(rb.LockAngularZ);

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

        const auto& cylinder = entity.GetComponent<CylinderColliderComponent>();
        CHECK_CLOSE(1.1f, cylinder.Radius, 1e-5f);
        CHECK_CLOSE(2.2f, cylinder.Height, 1e-5f);
        CHECK_VEC3_CLOSE(glm::vec3(1.0f, 1.1f, 1.2f), cylinder.Offset, 1e-5f);
        CHECK_CLOSE(4.5f, cylinder.Density, 1e-5f);
        CHECK_CLOSE(0.55f, cylinder.Friction, 1e-5f);
        CHECK_CLOSE(0.9f, cylinder.Restitution, 1e-5f);
    }
}
