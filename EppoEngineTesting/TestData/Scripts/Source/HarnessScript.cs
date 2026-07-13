using EppoScriptCore.Core;
using EppoScriptCore.Math;
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

        public override void OnCreate() => Created = 1;
        public override void OnUpdate(float deltaTime) => Accumulated += deltaTime;

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

        public bool Entity_HasComponent() => InternalCalls.Entity_HasComponent(ID, "PointLightComponent");
        public void Entity_AddComponent() => InternalCalls.Entity_AddComponent(ID, "PointLightComponent");
        public bool Entity_RemoveComponent() => InternalCalls.Entity_RemoveComponent(ID, "PointLightComponent");
        public bool Entity_GetName_Matches() => InternalCalls.Entity_GetName(ID) == "NamedEntity";
        public Vector3 TransformComponent_GetTranslation() => InternalCalls.TransformComponent_GetTranslation(ID);
        public void TransformComponent_SetTranslation(Vector3 t) => InternalCalls.TransformComponent_SetTranslation(ID, ref t);
        public ulong MeshComponent_GetMeshHandle() => InternalCalls.MeshComponent_GetMeshHandle(ID);
        public Vector3 PointLightComponent_GetColor() => InternalCalls.PointLightComponent_GetColor(ID);
        public void PointLightComponent_SetColor(Vector3 c) => InternalCalls.PointLightComponent_SetColor(ID, ref c);
        public float PointLightComponent_GetIntensity() => InternalCalls.PointLightComponent_GetIntensity(ID);
        public void PointLightComponent_SetIntensity(float intensity) => InternalCalls.PointLightComponent_SetIntensity(ID, intensity);
        public ulong RelationshipComponent_GetParent() => InternalCalls.RelationshipComponent_GetParent(ID);
        public void RelationshipComponent_SetParent(ulong parent) => InternalCalls.RelationshipComponent_SetParent(ID, parent);
    }
}
