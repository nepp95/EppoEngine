using EppoScriptCore.Core;
using EppoScriptCore.Math;
using EppoScriptCore.Physics;
using EppoScriptCore.Scene;

namespace EppoTesting
{
    // A minimal concrete script the Scripting suite asserts on. Kept small and
    // stable — the C++ tests depend on these names/types.
    public class HarnessScript : Entity
    {
        public float Speed = 2.5f;
        public int Count = 7;

        // Extra field types the marshalling round-trip tests assert on.
        public bool Enabled = true;
        public double Ratio = 1.5;
        public Vector3 Position;
        public Entity? Target;

        // Observable lifecycle side effects so the C++ tests can prove OnCreate /
        // OnUpdate actually route through managed and back via field reads.
        public int Created;
        public float Accumulated;

        // Set by the destroy-safety test: makes OnUpdate destroy this entity from
        // inside the script update loop, exercising deferred destruction.
        public bool DestroySelfOnUpdate;

        public override void OnCreate() => Created = 1;
        public override void OnUpdate(float deltaTime)
        {
            Accumulated += deltaTime;
            if (DestroySelfOnUpdate)
                Scene.DestroyEntity(this);
        }

        // Exercised by the GetMethod/InvokeMethod round-trip.
        public int Add(int a, int b) => a + b;

        // Exercised by the exception-safety test: proves a throw from user code
        // doesn't escape the UnmanagedCallersOnly boundary and kill the host.
        public int Throws() => throw new System.InvalidOperationException("boom from script");

        // Proves Entity's == / != are null-safe: Target defaults to null, so both
        // comparisons must resolve without dereferencing a null operand (this threw
        // an NRE before the operators guarded null).
        public bool NullEqualityIsSafe() => Target == null && !(Target != null);

        // --- Internal-call forwarders ---
        public void LogMessage()
            => InternalCalls.LogMessage((byte)LogLevel.Info, "HarnessScript.LogMessage: native Log callback reached");
        public bool Input_IsKeyPressed() => InternalCalls.Input_IsKeyPressed((ushort)KeyCode.Space);
        public bool Input_IsMouseButtonPressed() => InternalCalls.Input_IsMouseButtonPressed((ushort)MouseCode.ButtonRight);
        public Vector2 Input_GetMousePosition() => InternalCalls.Input_GetMousePosition();

        // The public Input facade rather than the raw internal call: GetMouseX/Y are
        // derived C#-side from GetMousePosition, so they need their own cover.
        public float Input_GetMouseX() => Input.GetMouseX();
        public float Input_GetMouseY() => Input.GetMouseY();

        public bool Entity_HasComponent() => InternalCalls.Entity_HasComponent(ID, "PointLightComponent");
        public void Entity_AddComponent() => InternalCalls.Entity_AddComponent(ID, "PointLightComponent");
        public bool Entity_RemoveComponent() => InternalCalls.Entity_RemoveComponent(ID, "PointLightComponent");
        public bool Entity_GetName_Matches() => InternalCalls.Entity_GetName(ID) == "NamedEntity";
        public void Entity_SetName() => InternalCalls.Entity_SetName(ID, "RenamedFromScript");
        public ulong Scene_CreateEntity() => Scene.CreateEntity("Spawned").ID;
        public void Scene_DestroyEntity(ulong id) => Scene.DestroyEntity(new Entity(id));
        public Vector3 TransformComponent_GetTranslation() => InternalCalls.TransformComponent_GetTranslation(ID);
        public void TransformComponent_SetTranslation(Vector3 t) => InternalCalls.TransformComponent_SetTranslation(ID, ref t);
        public Vector3 TransformComponent_GetRotation() => InternalCalls.TransformComponent_GetRotation(ID);
        public void TransformComponent_SetRotation(Vector3 r) => InternalCalls.TransformComponent_SetRotation(ID, ref r);
        public Vector3 TransformComponent_GetScale() => InternalCalls.TransformComponent_GetScale(ID);
        public void TransformComponent_SetScale(Vector3 s) => InternalCalls.TransformComponent_SetScale(ID, ref s);
        public ulong MeshComponent_GetMeshHandle() => InternalCalls.MeshComponent_GetMeshHandle(ID);
        public void MeshComponent_SetMeshHandle(ulong handle) => InternalCalls.MeshComponent_SetMeshHandle(ID, handle);

        // The public typed API rather than the raw handle: proves the enum maps onto
        // the reserved primitive handles the asset manager generates.
        public void MeshComponent_SetPrimitiveCube() => GetComponent<MeshComponent>().SetPrimitive(PrimitiveMesh.Cube);

        // Spawns an entity and makes it visible entirely from script, which is the
        // scenario writable mesh handles exist for.
        public ulong Scene_CreateEntityWithPrimitive()
        {
            Entity entity = Scene.CreateEntity("SpawnedVisible");
            entity.AddComponent<MeshComponent>().SetPrimitive(PrimitiveMesh.Sphere);
            return entity.ID;
        }
        public float CameraComponent_GetVerticalFov() => InternalCalls.CameraComponent_GetVerticalFov(ID);
        public void CameraComponent_SetVerticalFov(float v) => InternalCalls.CameraComponent_SetVerticalFov(ID, v);
        public float CameraComponent_GetNearClip() => InternalCalls.CameraComponent_GetNearClip(ID);
        public void CameraComponent_SetNearClip(float v) => InternalCalls.CameraComponent_SetNearClip(ID, v);
        public float CameraComponent_GetFarClip() => InternalCalls.CameraComponent_GetFarClip(ID);
        public void CameraComponent_SetFarClip(float v) => InternalCalls.CameraComponent_SetFarClip(ID, v);
        public Vector3 PointLightComponent_GetColor() => InternalCalls.PointLightComponent_GetColor(ID);
        public void PointLightComponent_SetColor(Vector3 c) => InternalCalls.PointLightComponent_SetColor(ID, ref c);
        public float PointLightComponent_GetIntensity() => InternalCalls.PointLightComponent_GetIntensity(ID);
        public void PointLightComponent_SetIntensity(float intensity) => InternalCalls.PointLightComponent_SetIntensity(ID, intensity);
        public ulong RelationshipComponent_GetParent() => InternalCalls.RelationshipComponent_GetParent(ID);
        public void RelationshipComponent_SetParent(ulong parent) => InternalCalls.RelationshipComponent_SetParent(ID, parent);
        public int RelationshipComponent_GetChildCount() => InternalCalls.RelationshipComponent_GetChildren(ID).Length;
        public ulong RelationshipComponent_GetChild(int index) => InternalCalls.RelationshipComponent_GetChildren(ID)[index];

        public byte RigidBodyComponent_GetType() => InternalCalls.RigidBodyComponent_GetType(ID);
        public void RigidBodyComponent_SetType(byte type) => InternalCalls.RigidBodyComponent_SetType(ID, type);
        public Vector3 RigidBodyComponent_GetLinearVelocity() => GetComponent<RigidBodyComponent>().LinearVelocity;
        public void RigidBodyComponent_SetLinearVelocity(Vector3 v) => GetComponent<RigidBodyComponent>().LinearVelocity = v;
        public float RigidBodyComponent_GetGravityScale() => InternalCalls.RigidBodyComponent_GetGravityScale(ID);
        public void RigidBodyComponent_SetGravityScale(float v) => InternalCalls.RigidBodyComponent_SetGravityScale(ID, v);
        public float RigidBodyComponent_GetLinearDamping() => InternalCalls.RigidBodyComponent_GetLinearDamping(ID);
        public void RigidBodyComponent_SetLinearDamping(float v) => InternalCalls.RigidBodyComponent_SetLinearDamping(ID, v);
        public float RigidBodyComponent_GetAngularDamping() => InternalCalls.RigidBodyComponent_GetAngularDamping(ID);
        public void RigidBodyComponent_SetAngularDamping(float v) => InternalCalls.RigidBodyComponent_SetAngularDamping(ID, v);
        public void Physics_ApplyLinearImpulseUp() => Physics.ApplyLinearImpulse(this, new Vector3(0.0f, 5.0f, 0.0f));

        public Vector3 BoxColliderComponent_GetHalfSize() => InternalCalls.BoxColliderComponent_GetHalfSize(ID);
        public void BoxColliderComponent_SetHalfSize(Vector3 v) => InternalCalls.BoxColliderComponent_SetHalfSize(ID, ref v);
        public Vector3 BoxColliderComponent_GetOffset() => InternalCalls.BoxColliderComponent_GetOffset(ID);
        public void BoxColliderComponent_SetOffset(Vector3 v) => InternalCalls.BoxColliderComponent_SetOffset(ID, ref v);
        public float BoxColliderComponent_GetDensity() => InternalCalls.BoxColliderComponent_GetDensity(ID);
        public void BoxColliderComponent_SetDensity(float v) => InternalCalls.BoxColliderComponent_SetDensity(ID, v);
        public float BoxColliderComponent_GetFriction() => InternalCalls.BoxColliderComponent_GetFriction(ID);
        public void BoxColliderComponent_SetFriction(float v) => InternalCalls.BoxColliderComponent_SetFriction(ID, v);
        public float BoxColliderComponent_GetRestitution() => InternalCalls.BoxColliderComponent_GetRestitution(ID);
        public void BoxColliderComponent_SetRestitution(float v) => InternalCalls.BoxColliderComponent_SetRestitution(ID, v);

        public float SphereColliderComponent_GetRadius() => InternalCalls.SphereColliderComponent_GetRadius(ID);
        public void SphereColliderComponent_SetRadius(float v) => InternalCalls.SphereColliderComponent_SetRadius(ID, v);
        public Vector3 SphereColliderComponent_GetOffset() => InternalCalls.SphereColliderComponent_GetOffset(ID);
        public void SphereColliderComponent_SetOffset(Vector3 v) => InternalCalls.SphereColliderComponent_SetOffset(ID, ref v);
        public float SphereColliderComponent_GetDensity() => InternalCalls.SphereColliderComponent_GetDensity(ID);
        public void SphereColliderComponent_SetDensity(float v) => InternalCalls.SphereColliderComponent_SetDensity(ID, v);
        public float SphereColliderComponent_GetFriction() => InternalCalls.SphereColliderComponent_GetFriction(ID);
        public void SphereColliderComponent_SetFriction(float v) => InternalCalls.SphereColliderComponent_SetFriction(ID, v);
        public float SphereColliderComponent_GetRestitution() => InternalCalls.SphereColliderComponent_GetRestitution(ID);
        public void SphereColliderComponent_SetRestitution(float v) => InternalCalls.SphereColliderComponent_SetRestitution(ID, v);

        public float CapsuleColliderComponent_GetRadius() => InternalCalls.CapsuleColliderComponent_GetRadius(ID);
        public void CapsuleColliderComponent_SetRadius(float v) => InternalCalls.CapsuleColliderComponent_SetRadius(ID, v);
        public float CapsuleColliderComponent_GetHeight() => InternalCalls.CapsuleColliderComponent_GetHeight(ID);
        public void CapsuleColliderComponent_SetHeight(float v) => InternalCalls.CapsuleColliderComponent_SetHeight(ID, v);
        public Vector3 CapsuleColliderComponent_GetOffset() => InternalCalls.CapsuleColliderComponent_GetOffset(ID);
        public void CapsuleColliderComponent_SetOffset(Vector3 v) => InternalCalls.CapsuleColliderComponent_SetOffset(ID, ref v);
        public float CapsuleColliderComponent_GetDensity() => InternalCalls.CapsuleColliderComponent_GetDensity(ID);
        public void CapsuleColliderComponent_SetDensity(float v) => InternalCalls.CapsuleColliderComponent_SetDensity(ID, v);
        public float CapsuleColliderComponent_GetFriction() => InternalCalls.CapsuleColliderComponent_GetFriction(ID);
        public void CapsuleColliderComponent_SetFriction(float v) => InternalCalls.CapsuleColliderComponent_SetFriction(ID, v);
        public float CapsuleColliderComponent_GetRestitution() => InternalCalls.CapsuleColliderComponent_GetRestitution(ID);
        public void CapsuleColliderComponent_SetRestitution(float v) => InternalCalls.CapsuleColliderComponent_SetRestitution(ID, v);
        public float CylinderColliderComponent_GetRadius() => InternalCalls.CylinderColliderComponent_GetRadius(ID);
        public void CylinderColliderComponent_SetRadius(float v) => InternalCalls.CylinderColliderComponent_SetRadius(ID, v);
        public float CylinderColliderComponent_GetHeight() => InternalCalls.CylinderColliderComponent_GetHeight(ID);
        public void CylinderColliderComponent_SetHeight(float v) => InternalCalls.CylinderColliderComponent_SetHeight(ID, v);
        public Vector3 CylinderColliderComponent_GetOffset() => InternalCalls.CylinderColliderComponent_GetOffset(ID);
        public void CylinderColliderComponent_SetOffset(Vector3 v) => InternalCalls.CylinderColliderComponent_SetOffset(ID, ref v);
        public float CylinderColliderComponent_GetDensity() => InternalCalls.CylinderColliderComponent_GetDensity(ID);
        public void CylinderColliderComponent_SetDensity(float v) => InternalCalls.CylinderColliderComponent_SetDensity(ID, v);
        public float CylinderColliderComponent_GetFriction() => InternalCalls.CylinderColliderComponent_GetFriction(ID);
        public void CylinderColliderComponent_SetFriction(float v) => InternalCalls.CylinderColliderComponent_SetFriction(ID, v);
        public float CylinderColliderComponent_GetRestitution() => InternalCalls.CylinderColliderComponent_GetRestitution(ID);
        public void CylinderColliderComponent_SetRestitution(float v) => InternalCalls.CylinderColliderComponent_SetRestitution(ID, v);
    }
}
