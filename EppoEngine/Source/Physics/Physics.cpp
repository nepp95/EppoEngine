#include "pch.h"
#include "Physics.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>


namespace Eppo
{
    static void TraceImpl(const char* inFMT, ...)
    {
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);

        EPPO_TRACE(buffer);
    }

    static bool FailedAssertImpl(const char* inExpression, const char* inMessage, const char* inFile, unsigned int inLine)
    {
        EPPO_ERROR("Jolt Assert: {}:{} ({}) {}", inFile, inLine, inExpression, inMessage);
        return true;
    }

    struct PhysicsData
    {
        const float FixedTimestep = 1.0f / 60.0f;
        const uint32_t MaxBodies = UINT16_MAX;
        const uint32_t NumBodyMutexes = 0;
        const uint32_t MaxBodyPairs = 1024;
        const uint32_t MaxContactRestraints = 1024;

        // We need a temp allocator for temporary allocations during the physics update. We're
        // pre-allocating 10 MB to avoid having to do allocations during the physics update.
        TempAllocatorImpl Allocator = TempAllocatorImpl(10 * 1024 * 1024);

        // We need a job system that will execute physics jobs on multiple threads. Typically
        // you would implement the JobSystem interface yourself and let Jolt Physics run on top
        // of your own job scheduler. JobSystemThreadPool is an example implementation.
        JobSystemThreadPool JobSystem = JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, static_cast<int>(std::thread::hardware_concurrency()) - 1);

        // Create mapping table from object layer to broadphase layer
        // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
        // Also have a look at BroadPhaseLayerInterfaceTable or BroadPhaseLayerInterfaceMask for a simpler interface.
        BroadPhaseLayerImpl BroadPhaseLayerInterface;

        // Create class that filters object vs broadphase layers
        // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
        // Also have a look at ObjectVsBroadPhaseLayerFilterTable or ObjectVsBroadPhaseLayerFilterMask for a simpler interface.
        ObjectVsBroadPhaseLayerFilterImpl ObjectVsBroadPhaseLayerFilterInterface;

        // Create class that filters object vs object layers
        // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
        // Also have a look at ObjectLayerPairFilterTable or ObjectLayerPairFilterMask for a simpler interface.
        ObjectLayerPairFilterImpl ObjectLayerPairFilterInterface;

        PhysicsSystem PhysicsSystem;
        BodyActivationListener ActivationListener;
        ContactListener ContactListener;
        BodyInterface* BodyInterface;
    };

    static PhysicsData* s_Data = nullptr;

    void Physics::Init()
    {
        s_Data = new PhysicsData();

        // Allocation hook - can be overridden
        RegisterDefaultAllocator();

        // Trace and assert callbacks
        Trace = TraceImpl;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = FailedAssertImpl);

        // Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
        Factory::sInstance = new Factory();

        // Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
        // If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
        // If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
        RegisterTypes();

        // Create physics system
        s_Data->PhysicsSystem.Init(s_Data->MaxBodies, s_Data->NumBodyMutexes, s_Data->MaxBodyPairs, s_Data->MaxContactRestraints, s_Data->BroadPhaseLayerInterface, s_Data->ObjectVsBroadPhaseLayerFilterInterface, s_Data->ObjectLayerPairFilterInterface);

        // A body activation listener gets notified when bodies activate and go to sleep
        // Note that this is called from a job so whatever you do here needs to be thread safe.
        // Registering one is entirely optional.
        s_Data->PhysicsSystem.SetBodyActivationListener(&s_Data->ActivationListener);

        // A contact listener gets notified when bodies (are about to) collide, and when they separate again.
        // Note that this is called from a job so whatever you do here needs to be thread safe.
        // Registering one is entirely optional.
        s_Data->PhysicsSystem.SetContactListener(&s_Data->ContactListener);

        // We use this A LOT
        s_Data->BodyInterface = &s_Data->PhysicsSystem.GetBodyInterface();
    }

    void Physics::Shutdown()
    {
        // Cleanup physics
        UnregisterTypes();

        delete Factory::sInstance;
        Factory::sInstance = nullptr;

        delete s_Data;
    }

    void Physics::Update(const uint32_t bodyId)
    {
        if (s_Data->BodyInterface->IsActive(BodyID(bodyId)))
            s_Data->PhysicsSystem.Update(s_Data->FixedTimestep, 1, &s_Data->Allocator, &s_Data->JobSystem);
    }

    uint32_t Physics::CreateRigidBody(const BodyShape shape, const bool isDynamic)
    {
        ShapeSettings::ShapeResult shapeResult;

        switch (shape)
        {
            case BodyShape::Box:
            {
                const auto boxSettings = new BoxShapeSettings(Vec3(1.0f, 1.0f, 1.0f));
                shapeResult = boxSettings->Create();
                break;
            }
        }

        const ShapeRefC shapeRef = shapeResult.Get();
        const BodyCreationSettings settings(shapeRef, JPH::RVec3(0.0f, 0.0f, 0.0f), JPH::Quat::sIdentity(), JPH::EMotionType::Static, 0);

        const auto bodyInterface = s_Data->BodyInterface;
        const Body* body = bodyInterface->CreateBody(settings);
        bodyInterface->AddBody(body->GetID(), isDynamic ? EActivation::Activate : EActivation::DontActivate);

        // TODO: Don't do this every time... Expensive operation
        s_Data->PhysicsSystem.OptimizeBroadPhase();

        return body->GetID().GetIndexAndSequenceNumber();
    }

    void Physics::RemoveRigidBody(const uint32_t bodyId)
    {
        const auto id = BodyID(bodyId);

        s_Data->BodyInterface->RemoveBody(id);
        s_Data->BodyInterface->DestroyBody(id);
    }

    void Physics::ApplyLinearImpulse(const uint32_t bodyId, const glm::vec3& impulse)
    {
        s_Data->BodyInterface->AddImpulse(BodyID(bodyId), { impulse.x, impulse.y, impulse.z });
    }

    void Physics::ApplyLinearImpulse(const uint32_t bodyId, const glm::vec3& impulse, const glm::vec3& worldPosition)
    {
        s_Data->BodyInterface->AddImpulse(BodyID(bodyId), { impulse.x, impulse.y, impulse.z }, { worldPosition.x, worldPosition.y, worldPosition.z });
    }

    glm::vec3 Physics::GetRigidBodyPosition(const uint32_t bodyId)
    {
        const auto id = BodyID(bodyId);
        const auto position = s_Data->BodyInterface->GetPosition(id);

        return { position.GetX(), position.GetY(), position.GetZ() };
    }

    glm::vec3 Physics::GetRigidBodyRotation(const uint32_t bodyId)
    {
        const auto id = BodyID(bodyId);
        const auto rotation = s_Data->BodyInterface->GetRotation(id).GetEulerAngles();

        return { rotation.GetX(), rotation.GetY(), rotation.GetZ() };
    }
}
