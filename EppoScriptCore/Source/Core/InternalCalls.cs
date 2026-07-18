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
            => ((delegate* unmanaged[Cdecl]<ushort, byte>)Get("Input_IsKeyPressed"))(key) != 0;
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

        internal static ulong MeshComponent_GetMeshHandle(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, ulong>)Get("MeshComponent_GetMeshHandle"))(id);
        }

        internal static Vector3 PointLightComponent_GetColor(ulong id)
        {
            Vector3 result = default;
            ((delegate* unmanaged[Cdecl]<ulong, Vector3*, void>)Get("PointLightComponent_GetColor"))(id, &result);
            return result;
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

        internal static byte RigidBodyComponent_GetType(ulong id)
        {
            return ((delegate* unmanaged[Cdecl]<ulong, byte>)Get("RigidBodyComponent_GetType"))(id);
        }

        internal static void RigidBodyComponent_SetType(ulong id, byte type)
        {
            ((delegate* unmanaged[Cdecl]<ulong, byte, void>)Get("RigidBodyComponent_SetType"))(id, type);
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
        #endregion
    }
}
