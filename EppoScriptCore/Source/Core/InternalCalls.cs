using System.Runtime.InteropServices;
using EppoScriptCore.Math;

namespace EppoScriptCore.Core
{
    // Registry of native engine functions exposed to managed scripts. Native
    // registers each by name during bootstrap (before any script runs); the typed
    // wrappers below resolve the raw pointer with Get and invoke it via a C#
    // unmanaged function pointer. Get throws if a name was never registered, so a
    // missing callback surfaces as a caught exception at the boundary, not a null
    // call.
    //
    // This is the single unsafe class: all delegate* unmanaged[Cdecl] calls live
    // here so callers (Entity, Input, component wrappers) stay plain safe code.
    internal static unsafe class InternalCalls
    {
        private static readonly Dictionary<string, IntPtr> s_Calls = new();

        internal static void Register(string name, IntPtr function) => s_Calls[name] = function;

        internal static IntPtr Get(string name)
            => s_Calls.TryGetValue(name, out var fn) && fn != IntPtr.Zero
                ? fn
                : throw new InvalidOperationException($"Internal call '{name}' is not registered.");

        #region Core
        internal static void LogMessage(byte level, string message)
        {
            var ptr = Marshal.StringToCoTaskMemUTF8(message);
            try
            {
                ((delegate* unmanaged[Cdecl]<byte, IntPtr, void>)Get("LogMessage"))(level, ptr);
            }
            finally
            {
                Marshal.FreeCoTaskMem(ptr);
            }
        }

        internal static bool Input_IsKeyPressed(ushort key)
        {
            return ((delegate* unmanaged[Cdecl]<ushort, byte>)Get("Input_IsKeyPressed"))(key) != 0;
        }

        internal static bool Input_IsMouseButtonPressed(ushort button)
        {
            return ((delegate* unmanaged[Cdecl]<ushort, byte>)Get("Input_IsMouseButtonPressed"))(button) != 0;
        }

        internal static Vector2 Input_GetMousePosition()
        {
            Vector2 position = default;
            ((delegate* unmanaged[Cdecl]<Vector2*, void>)Get("Input_GetMousePosition"))(&position);
            return position;
        }
        #endregion

        #region Physics
        internal static void Physics_ApplyLinearImpulse(ulong id, ref Vector3 impulse)
        {
            fixed (Vector3* ptr = &impulse)
                ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("Physics_ApplyLinearImpulse"))(id, ptr);
        }

        internal static void Physics_GetLinearVelocity(ulong id, out Vector3 velocity)
        {
            velocity = default;
            fixed (Vector3* ptr = &velocity)
                ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("Physics_GetLinearVelocity"))(id, ptr);
        }

        internal static void Physics_SetLinearVelocity(ulong id, ref Vector3 velocity)
        {
            fixed (Vector3* ptr = &velocity)
                ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("Physics_SetLinearVelocity"))(id, ptr);
        }

        // Blittable ray-cast result; layout must match ScriptRayHit in ScriptGlue.cpp.
        [StructLayout(LayoutKind.Sequential)]
        internal struct RaycastHitNative
        {
            public Vector3 Point;
            public Vector3 Normal;
            public ulong EntityId;
            public float Distance;
            public byte Hit;
        }

        internal static void Physics_Raycast(ref Vector3 origin, ref Vector3 direction, float maxDistance, out RaycastHitNative result)
        {
            result = default;
            fixed (Vector3* o = &origin)
            fixed (Vector3* d = &direction)
            fixed (RaycastHitNative* r = &result)
                ((delegate* unmanaged[Cdecl]<Vector3*, Vector3*, float, RaycastHitNative*, void>)Get("Physics_Raycast"))(o, d, maxDistance, r);
        }

        internal static bool Physics_OverlapsSphere(ulong id, ref Vector3 center, float radius)
        {
            fixed (Vector3* c = &center)
                return ((delegate* unmanaged[Cdecl]<ulong, Vector3*, float, byte>)Get("Physics_OverlapsSphere"))(id, c, radius) != 0;
        }
        #endregion

        #region Scene
        internal static bool Entity_HasComponent(ulong id, string typeName)
        {
            var ptr = Marshal.StringToCoTaskMemUTF8(typeName);
            try
            {
                return ((delegate* unmanaged[Cdecl]<ulong, byte*, byte>)Get("Entity_HasComponent"))(id, (byte*)ptr) != 0;
            }
            finally
            {
                Marshal.FreeCoTaskMem(ptr);
            }
        }

        internal static void Entity_AddComponent(ulong id, string typeName)
        {
            var ptr = Marshal.StringToCoTaskMemUTF8(typeName);
            try
            {
                ((delegate* unmanaged[Cdecl]<ulong, byte*, void>)Get("Entity_AddComponent"))(id, (byte*)ptr);
            }
            finally
            {
                Marshal.FreeCoTaskMem(ptr);
            }
        }

        internal static bool Entity_RemoveComponent(ulong id, string typeName)
        {
            var ptr = Marshal.StringToCoTaskMemUTF8(typeName);
            try
            {
                return ((delegate* unmanaged[Cdecl]<ulong, byte*, byte>)Get("Entity_RemoveComponent"))(id, (byte*)ptr) != 0;
            }
            finally
            {
                Marshal.FreeCoTaskMem(ptr);
            }
        }

        internal static string Entity_GetName(ulong id)
        {
            // Native returns a pointer to a UTF-8 string it still owns; PtrToStringUTF8
            // copies it into a managed string here, before the pointer can go stale.
            var ptr = ((delegate* unmanaged[Cdecl]<ulong, IntPtr>)Get("Entity_GetName"))(id);
            return Marshal.PtrToStringUTF8(ptr) ?? string.Empty;
        }

        internal static void Entity_SetName(ulong id, string name)
        {
            var ptr = Marshal.StringToCoTaskMemUTF8(name);
            try
            {
                ((delegate* unmanaged[Cdecl]<ulong, byte*, void>)Get("Entity_SetName"))(id, (byte*)ptr);
            }
            finally
            {
                Marshal.FreeCoTaskMem(ptr);
            }
        }

        internal static ulong Scene_CreateEntity(string name)
        {
            var ptr = Marshal.StringToCoTaskMemUTF8(name);
            try
            {
                return ((delegate* unmanaged[Cdecl]<byte*, ulong>)Get("Scene_CreateEntity"))((byte*)ptr);
            }
            finally
            {
                Marshal.FreeCoTaskMem(ptr);
            }
        }

        internal static void Scene_DestroyEntity(ulong id)
        {
            ((delegate* unmanaged[Cdecl]<ulong, void>)Get("Scene_DestroyEntity"))(id);
        }

        internal static ulong Scene_FindEntityByName(string name)
        {
            var ptr = Marshal.StringToCoTaskMemUTF8(name);
            try
            {
                return ((delegate* unmanaged[Cdecl]<byte*, ulong>)Get("Scene_FindEntityByName"))((byte*)ptr);
            }
            finally
            {
                Marshal.FreeCoTaskMem(ptr);
            }
        }

        internal static Vector3 TransformComponent_GetTranslation(ulong id)
        {
            Vector3 result = default;
            ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("TransformComponent_GetTranslation"))(id, &result);
            return result;
        }

        internal static void TransformComponent_SetTranslation(ulong id, ref Vector3 translation)
        {
            fixed (Vector3* ptr = &translation)
                ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("TransformComponent_SetTranslation"))(id, ptr);
        }

        internal static Vector3 TransformComponent_GetRotation(ulong id)
        {
            Vector3 result = default;
            ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("TransformComponent_GetRotation"))(id, &result);
            return result;
        }

        internal static void TransformComponent_SetRotation(ulong id, ref Vector3 rotation)
        {
            fixed (Vector3* ptr = &rotation)
                ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("TransformComponent_SetRotation"))(id, ptr);
        }

        internal static Vector3 TransformComponent_GetScale(ulong id)
        {
            Vector3 result = default;
            ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("TransformComponent_GetScale"))(id, &result);
            return result;
        }

        internal static void TransformComponent_SetScale(ulong id, ref Vector3 scale)
        {
            fixed (Vector3* ptr = &scale)
                ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("TransformComponent_SetScale"))(id, ptr);
        }

        internal static ulong MeshComponent_GetMeshHandle(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, ulong>)Get("MeshComponent_GetMeshHandle"))(id);
        }

        internal static void MeshComponent_SetMeshHandle(ulong id, ulong meshHandle)
        {
            ((delegate* unmanaged[Cdecl]<ulong, ulong, void>)Get("MeshComponent_SetMeshHandle"))(id, meshHandle);
        }

        internal static Vector3 PointLightComponent_GetColor(ulong id)
        {
            Vector3 result = default;
            ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("PointLightComponent_GetColor"))(id, &result);
            return result;
        }

        internal static bool CameraComponent_GetPrimary(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, bool>)Get("CameraComponent_GetPrimary"))(id);
        }

        internal static void CameraComponent_SetPrimary(ulong id, bool primary)
        {
            ((delegate* unmanaged[Cdecl]<ulong, bool, void>)Get("CameraComponent_SetPrimary"))(id, primary);
        }

        internal static float CameraComponent_GetVerticalFov(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CameraComponent_GetVerticalFov"))(id);
        }

        internal static void CameraComponent_SetVerticalFov(ulong id, float verticalFov)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CameraComponent_SetVerticalFov"))(id, verticalFov);
        }

        internal static float CameraComponent_GetNearClip(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CameraComponent_GetNearClip"))(id);
        }

        internal static void CameraComponent_SetNearClip(ulong id, float nearClip)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CameraComponent_SetNearClip"))(id, nearClip);
        }

        internal static float CameraComponent_GetFarClip(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CameraComponent_GetFarClip"))(id);
        }

        internal static void CameraComponent_SetFarClip(ulong id, float farClip)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CameraComponent_SetFarClip"))(id, farClip);
        }

        internal static void PointLightComponent_SetColor(ulong id, ref Vector3 translation)
        {
            fixed (Vector3* ptr = &translation)
                ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("PointLightComponent_SetColor"))(id, ptr);
        }

        internal static float PointLightComponent_GetIntensity(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("PointLightComponent_GetIntensity"))(id);
        }

        internal static void PointLightComponent_SetIntensity(ulong id, float intensity)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("PointLightComponent_SetIntensity"))(id, intensity);
        }

        internal static ulong RelationshipComponent_GetParent(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, ulong>)Get("RelationshipComponent_GetParent"))(id);
        }

        internal static void RelationshipComponent_SetParent(ulong id, ulong parent)
        {
            ((delegate* unmanaged[Cdecl]<ulong, ulong, void>)Get("RelationshipComponent_SetParent"))(id, parent);
        }

        internal static ulong[] RelationshipComponent_GetChildren(ulong id)
        {
            var count = ((delegate* unmanaged[Cdecl]<ulong, int>)Get("RelationshipComponent_GetChildCount"))(id);
            var children = new ulong[count < 0 ? 0 : count];
            for (var i = 0; i < children.Length; i++)
                children[i] = ((delegate* unmanaged[Cdecl]<ulong, int, ulong>)Get("RelationshipComponent_GetChild"))(id, i);
            return children;
        }

        internal static byte RigidBodyComponent_GetType(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, byte>)Get("RigidBodyComponent_GetType"))(id);
        }

        internal static void RigidBodyComponent_SetType(ulong id, byte type)
        {
            ((delegate* unmanaged[Cdecl]<ulong, byte, void>)Get("RigidBodyComponent_SetType"))(id, type);
        }

        internal static float RigidBodyComponent_GetGravityScale(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("RigidBodyComponent_GetGravityScale"))(id);
        }

        internal static void RigidBodyComponent_SetGravityScale(ulong id, float gravityScale)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("RigidBodyComponent_SetGravityScale"))(id, gravityScale);
        }

        internal static float RigidBodyComponent_GetLinearDamping(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("RigidBodyComponent_GetLinearDamping"))(id);
        }

        internal static void RigidBodyComponent_SetLinearDamping(ulong id, float linearDamping)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("RigidBodyComponent_SetLinearDamping"))(id, linearDamping);
        }

        internal static float RigidBodyComponent_GetAngularDamping(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("RigidBodyComponent_GetAngularDamping"))(id);
        }

        internal static void RigidBodyComponent_SetAngularDamping(ulong id, float angularDamping)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("RigidBodyComponent_SetAngularDamping"))(id, angularDamping);
        }

        internal static Vector3 BoxColliderComponent_GetHalfSize(ulong id)
        {
            Vector3 result = default;
            ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("BoxColliderComponent_GetHalfSize"))(id, &result);
            return result;
        }

        internal static void BoxColliderComponent_SetHalfSize(ulong id, ref Vector3 halfSize)
        {
            fixed (Vector3* ptr = &halfSize)
                ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("BoxColliderComponent_SetHalfSize"))(id, ptr);
        }

        internal static Vector3 BoxColliderComponent_GetOffset(ulong id)
        {
            Vector3 result = default;
            ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("BoxColliderComponent_GetOffset"))(id, &result);
            return result;
        }

        internal static void BoxColliderComponent_SetOffset(ulong id, ref Vector3 offset)
        {
            fixed (Vector3* ptr = &offset)
                ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("BoxColliderComponent_SetOffset"))(id, ptr);
        }

        internal static float BoxColliderComponent_GetDensity(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("BoxColliderComponent_GetDensity"))(id);
        }

        internal static void BoxColliderComponent_SetDensity(ulong id, float density)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("BoxColliderComponent_SetDensity"))(id, density);
        }

        internal static float BoxColliderComponent_GetFriction(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("BoxColliderComponent_GetFriction"))(id);
        }

        internal static void BoxColliderComponent_SetFriction(ulong id, float friction)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("BoxColliderComponent_SetFriction"))(id, friction);
        }

        internal static float BoxColliderComponent_GetRestitution(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("BoxColliderComponent_GetRestitution"))(id);
        }

        internal static void BoxColliderComponent_SetRestitution(ulong id, float restitution)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("BoxColliderComponent_SetRestitution"))(id, restitution);
        }

        internal static float SphereColliderComponent_GetRadius(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("SphereColliderComponent_GetRadius"))(id);
        }

        internal static void SphereColliderComponent_SetRadius(ulong id, float radius)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("SphereColliderComponent_SetRadius"))(id, radius);
        }

        internal static Vector3 SphereColliderComponent_GetOffset(ulong id)
        {
            Vector3 result = default;
            ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("SphereColliderComponent_GetOffset"))(id, &result);
            return result;
        }

        internal static void SphereColliderComponent_SetOffset(ulong id, ref Vector3 offset)
        {
            fixed (Vector3* ptr = &offset)
                ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("SphereColliderComponent_SetOffset"))(id, ptr);
        }

        internal static float SphereColliderComponent_GetDensity(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("SphereColliderComponent_GetDensity"))(id);
        }

        internal static void SphereColliderComponent_SetDensity(ulong id, float density)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("SphereColliderComponent_SetDensity"))(id, density);
        }

        internal static float SphereColliderComponent_GetFriction(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("SphereColliderComponent_GetFriction"))(id);
        }

        internal static void SphereColliderComponent_SetFriction(ulong id, float friction)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("SphereColliderComponent_SetFriction"))(id, friction);
        }

        internal static float SphereColliderComponent_GetRestitution(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("SphereColliderComponent_GetRestitution"))(id);
        }

        internal static void SphereColliderComponent_SetRestitution(ulong id, float restitution)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("SphereColliderComponent_SetRestitution"))(id, restitution);
        }

        internal static float CapsuleColliderComponent_GetRadius(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CapsuleColliderComponent_GetRadius"))(id);
        }

        internal static void CapsuleColliderComponent_SetRadius(ulong id, float radius)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CapsuleColliderComponent_SetRadius"))(id, radius);
        }

        internal static float CapsuleColliderComponent_GetHeight(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CapsuleColliderComponent_GetHeight"))(id);
        }

        internal static void CapsuleColliderComponent_SetHeight(ulong id, float height)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CapsuleColliderComponent_SetHeight"))(id, height);
        }

        internal static Vector3 CapsuleColliderComponent_GetOffset(ulong id)
        {
            Vector3 result = default;
            ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("CapsuleColliderComponent_GetOffset"))(id, &result);
            return result;
        }

        internal static void CapsuleColliderComponent_SetOffset(ulong id, ref Vector3 offset)
        {
            fixed (Vector3* ptr = &offset)
                ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("CapsuleColliderComponent_SetOffset"))(id, ptr);
        }

        internal static float CapsuleColliderComponent_GetDensity(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CapsuleColliderComponent_GetDensity"))(id);
        }

        internal static void CapsuleColliderComponent_SetDensity(ulong id, float density)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CapsuleColliderComponent_SetDensity"))(id, density);
        }

        internal static float CapsuleColliderComponent_GetFriction(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CapsuleColliderComponent_GetFriction"))(id);
        }

        internal static void CapsuleColliderComponent_SetFriction(ulong id, float friction)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CapsuleColliderComponent_SetFriction"))(id, friction);
        }

        internal static float CapsuleColliderComponent_GetRestitution(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CapsuleColliderComponent_GetRestitution"))(id);
        }

        internal static void CapsuleColliderComponent_SetRestitution(ulong id, float restitution)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CapsuleColliderComponent_SetRestitution"))(id, restitution);
        }

        internal static float CylinderColliderComponent_GetRadius(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CylinderColliderComponent_GetRadius"))(id);
        }

        internal static void CylinderColliderComponent_SetRadius(ulong id, float radius)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CylinderColliderComponent_SetRadius"))(id, radius);
        }

        internal static float CylinderColliderComponent_GetHeight(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CylinderColliderComponent_GetHeight"))(id);
        }

        internal static void CylinderColliderComponent_SetHeight(ulong id, float height)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CylinderColliderComponent_SetHeight"))(id, height);
        }

        internal static Vector3 CylinderColliderComponent_GetOffset(ulong id)
        {
            Vector3 result = default;
            ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("CylinderColliderComponent_GetOffset"))(id, &result);
            return result;
        }

        internal static void CylinderColliderComponent_SetOffset(ulong id, ref Vector3 offset)
        {
            fixed (Vector3* ptr = &offset)
                ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("CylinderColliderComponent_SetOffset"))(id, ptr);
        }

        internal static float CylinderColliderComponent_GetDensity(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CylinderColliderComponent_GetDensity"))(id);
        }

        internal static void CylinderColliderComponent_SetDensity(ulong id, float density)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CylinderColliderComponent_SetDensity"))(id, density);
        }

        internal static float CylinderColliderComponent_GetFriction(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CylinderColliderComponent_GetFriction"))(id);
        }

        internal static void CylinderColliderComponent_SetFriction(ulong id, float friction)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CylinderColliderComponent_SetFriction"))(id, friction);
        }

        internal static float CylinderColliderComponent_GetRestitution(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("CylinderColliderComponent_GetRestitution"))(id);
        }

        internal static void CylinderColliderComponent_SetRestitution(ulong id, float restitution)
        {
            ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("CylinderColliderComponent_SetRestitution"))(id, restitution);
        }
        #endregion
    }
}
