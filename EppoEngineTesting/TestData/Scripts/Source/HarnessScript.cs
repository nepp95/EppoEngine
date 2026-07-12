using EppoScriptCore.Core;
using EppoScriptCore.Math;
using EppoScriptCore.Scene;

namespace EppoTesting
{
    // A minimal concrete script the Scripting suite asserts on. Kept small and
    // stable — the C++ tests depend on these names/types.
    public class HarnessScript : ScriptBehaviour
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

        public override void OnCreate() => Created = 1;
        public override void OnUpdate(float deltaTime) => Accumulated += deltaTime;

        // Exercised by the GetMethod/InvokeMethod round-trip.
        public int Add(int a, int b) => a + b;

        // Exercised by the exception-safety test: proves a throw from user code
        // doesn't escape the UnmanagedCallersOnly boundary and kill the host.
        public int Throws() => throw new System.InvalidOperationException("boom from script");

        // --- Internal-call forwarders (1:1 with ScriptGlue). ---

        // Core. Type-name string args are baked in because a string can't cross the
        // packed-arg C++ -> managed boundary; the id comes from ScriptBehaviour.Id.
        public void LogMessage()
            => InternalCalls.LogMessage((byte)LogLevel.Info, "HarnessScript.LogMessage: native Log callback reached");
        public bool Input_IsKeyPressed() => InternalCalls.Input_IsKeyPressed((ushort)KeyCode.Space);

        // Entity component registry.
        public bool Entity_HasComponent() => InternalCalls.Entity_HasComponent(Id, "PointLightComponent");
        public void Entity_AddComponent() => InternalCalls.Entity_AddComponent(Id, "PointLightComponent");
        public bool Entity_RemoveComponent() => InternalCalls.Entity_RemoveComponent(Id, "PointLightComponent");

        // TransformComponent.
        public Vector3 TransformComponent_GetTranslation() => InternalCalls.TransformComponent_GetTranslation(Id);
        public void TransformComponent_SetTranslation(Vector3 t) => InternalCalls.TransformComponent_SetTranslation(Id, ref t);

        // MeshComponent.
        public ulong MeshComponent_GetMeshHandle() => InternalCalls.MeshComponent_GetMeshHandle(Id);

        // PointLightComponent.
        public Vector3 PointLightComponent_GetColor() => InternalCalls.PointLightComponent_GetColor(Id);
        public void PointLightComponent_SetColor(Vector3 c) => InternalCalls.PointLightComponent_SetColor(Id, ref c);
        public float PointLightComponent_GetIntensity() => InternalCalls.PointLightComponent_GetIntensity(Id);
        public void PointLightComponent_SetIntensity(float intensity) => InternalCalls.PointLightComponent_SetIntensity(Id, intensity);

        // RelationshipComponent.
        public ulong RelationshipComponent_GetParent() => InternalCalls.RelationshipComponent_GetParent(Id);
        public void RelationshipComponent_SetParent(ulong parent) => InternalCalls.RelationshipComponent_SetParent(Id, parent);
    }
}
