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
        #endregion
    }
}
