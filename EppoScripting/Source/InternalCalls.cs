using System;
using System.Runtime.CompilerServices;

namespace Eppo
{
	public static class InternalCalls
	{
		// Core
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Log(LogLevel logLevel, string message);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsKeyPressed(KeyCode keyCode);

		// Scene
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Entity_AddComponent(ulong uuid, string componentType);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong Entity_CreateNewEntity();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong Entity_CreateNewEntity(string name);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string Entity_GetName(ulong uuid);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong Entity_FindEntityByName(string name);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Entity_HasComponent(ulong uuid, Type componentType);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TransformComponent_GetTranslation(ulong uuid, out Vector3 translation);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TransformComponent_SetTranslation(ulong uuid, ref Vector3 translation);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void RigidBodyComponent_ApplyLinearImpulse(ulong uuid, ref Vector3 impulse, ref Vector3 worldPosition);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void RigidBodyComponent_ApplyLinearImpulseToCenter(ulong uuid, ref Vector3 impulse);

		// ScriptEngine
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static object GetScriptInstance(ulong uuid);
	}
}
