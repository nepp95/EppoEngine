#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

using namespace JPH;

namespace Eppo
{
    enum class BodyShape
    {
        Box = 0
    };

    class Physics
    {
    public:
        static void Init();
        static void Shutdown();

        static void Update(uint32_t bodyId);

        static uint32_t CreateRigidBody(BodyShape shape, bool isDynamic);
        static void RemoveRigidBody(uint32_t bodyId);

        static void ApplyLinearImpulse(uint32_t bodyId, const glm::vec3& impulse);
        static void ApplyLinearImpulse(uint32_t bodyId, const glm::vec3& impulse, const glm::vec3& worldPosition);

        static glm::vec3 GetRigidBodyPosition(uint32_t bodyId);
        static glm::vec3 GetRigidBodyRotation(uint32_t bodyId);
    };

    class BroadPhaseLayerImpl final : public BroadPhaseLayerInterface
    {
    public:
        BroadPhaseLayerImpl()
        {
            m_ObjectToBroadPhase[0] = BroadPhaseLayer(0);
            m_ObjectToBroadPhase[1] = BroadPhaseLayer(1);
        }

        [[nodiscard]] unsigned int GetNumBroadPhaseLayers() const override
        {
            return 2;
        }

        [[nodiscard]] BroadPhaseLayer GetBroadPhaseLayer(const ObjectLayer layer) const override
        {
            JPH_ASSERT(layer < 2);
            return m_ObjectToBroadPhase[layer];
        }

        #if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        [[nodiscard]] const char* GetBroadPhaseLayerName(const BroadPhaseLayer layer) const override
        {
            switch (static_cast<BroadPhaseLayer::Type>(layer))
            {
                case 0: return "NON_MOVING";
                case 1: return "MOVING";

                default:
                {
                    JPH_ASSERT(false);
                    return "INVALID";
                }
            }
        }
        #endif

    private:
        BroadPhaseLayer m_ObjectToBroadPhase[2];
    };

    class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
    {
    public:
        [[nodiscard]] bool ShouldCollide(const ObjectLayer layerA, const BroadPhaseLayer layerB) const override
        {
            switch (layerA)
            {
                case 0: return static_cast<BroadPhaseLayer::Type>(layerB) == 2;
                case 1: return true;

                default:
                {
                    JPH_ASSERT(false);
                    return false;
                }
            }
        }
    };

    class ObjectLayerPairFilterImpl final : public ObjectLayerPairFilter
    {
    public:
        [[nodiscard]] bool ShouldCollide(const ObjectLayer objectA, const ObjectLayer objectB) const override
        {
            switch (objectA)
            {
                case 0: return objectB == 1;
                case 1: return true;

                default:
                {
                    JPH_ASSERT(false);
                    return false;
                }
            }
        }
    };

    class BodyActivationListener final : public JPH::BodyActivationListener
    {
    public:
        void OnBodyActivated(const BodyID& bodyID, uint64 bodyUserData) override
        {
            EPPO_TRACE("Body got activated", bodyID.GetIndexAndSequenceNumber());
        }

        void OnBodyDeactivated(const BodyID& bodyID, uint64 bodyUserData) override
        {
            EPPO_TRACE("Body got deactivated", bodyID.GetIndexAndSequenceNumber());
        }
    };

    class ContactListener final : public JPH::ContactListener
    {
    public:
        ValidateResult OnContactValidate(const Body& bodyA, const Body& bodyB, RVec3Arg baseOffset, const CollideShapeResult& collisionResult) override
        {
            EPPO_TRACE("Contact validate callback");
            return ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        void OnContactAdded(const Body& bodyA, const Body& bodyB, const ContactManifold& manifold, ContactSettings& ioSettings) override
        {
            EPPO_TRACE("A contact was added");
        }

        void OnContactPersisted(const Body& bodyA, const Body& bodyB, const ContactManifold& manifold, ContactSettings& ioSettings) override
        {
            EPPO_TRACE("A contact was persisted");
        }

        void OnContactRemoved(const SubShapeIDPair& pair) override
        {
            EPPO_TRACE("A contact was removed");
        }
    };
}