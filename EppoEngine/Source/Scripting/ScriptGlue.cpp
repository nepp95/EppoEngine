#include "pch.h"
#include "ScriptGlue.h"

#include "Core/Input.h"
#include "Scene/Entity.h"
#include "Scripting/ScriptEngine.h"

#include <mono/metadata/reflection.h>
#include <bullet/btBulletDynamicsCommon.h>

namespace Eppo
{
	static std::unordered_map<MonoType*, std::function<bool(Entity)>> s_EntityHasComponentFns;

	#define EPPO_ADD_INTERNAL_CALL(fn) mono_add_internal_call("Eppo.InternalCalls::"#fn, fn);

	static void Log(uint32_t logLevel, MonoString* message)
	{
		char* cStr = mono_string_to_utf8(message);
		std::string messageStr(cStr);
		mono_free(cStr);

		switch (logLevel)
		{
			case 0: EPPO_SCRIPT_TRACE(messageStr); break;
			case 1: EPPO_SCRIPT_INFO(messageStr); break;
			case 2: EPPO_SCRIPT_WARN(messageStr); break;
			case 3: EPPO_SCRIPT_ERROR(messageStr); break;
		}
	}

	static bool Input_IsKeyPressed(KeyCode keyCode)
	{
		return Input::IsKeyPressed(keyCode);
	}

	static bool Entity_HasComponent(UUID uuid, MonoReflectionType* componentType)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		MonoType* managedType = mono_reflection_type_get_type(componentType);
		auto it = s_EntityHasComponentFns.find(managedType);
		EPPO_ASSERT(it != s_EntityHasComponentFns.end());
		return it->second(entity);
	}

	static void TransformComponent_GetTranslation(UUID uuid, glm::vec3* outTranslation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		*outTranslation = entity.GetComponent<TransformComponent>().Translation;
	}

	static void TransformComponent_SetTranslation(UUID uuid, glm::vec3* translation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		entity.GetComponent<TransformComponent>().Translation = *translation;
	}

	static void RigidBodyComponent_ApplyLinearImpulse(UUID uuid, glm::vec3* impulse, glm::vec3* worldPosition)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		auto& rb = entity.GetComponent<RigidBodyComponent>();
		rb.RuntimeBody.ApplyLinearImpulse(*impulse, *worldPosition);
	}

	static void RigidBodyComponent_ApplyLinearImpulseToCenter(UUID uuid, glm::vec3* impulse)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		auto& rb = entity.GetComponent<RigidBodyComponent>();
		rb.RuntimeBody.ApplyLinearImpulse(*impulse);
	}

	void ScriptGlue::RegisterFunctions()
	{
		EPPO_ADD_INTERNAL_CALL(Log);
		EPPO_ADD_INTERNAL_CALL(Input_IsKeyPressed);
		EPPO_ADD_INTERNAL_CALL(Entity_HasComponent);
		EPPO_ADD_INTERNAL_CALL(TransformComponent_GetTranslation);
		EPPO_ADD_INTERNAL_CALL(TransformComponent_SetTranslation);
		EPPO_ADD_INTERNAL_CALL(RigidBodyComponent_ApplyLinearImpulse);
		EPPO_ADD_INTERNAL_CALL(RigidBodyComponent_ApplyLinearImpulseToCenter);
	}

	void ScriptGlue::RegisterComponents()
	{
		s_EntityHasComponentFns.clear();

		RegisterComponent<TransformComponent>();
		RegisterComponent<SpriteComponent>();
		RegisterComponent<MeshComponent>();
		RegisterComponent<DirectionalLightComponent>();
		RegisterComponent<ScriptComponent>();
		RegisterComponent<RigidBodyComponent>();
	}

	template<typename T>
	void ScriptGlue::RegisterComponent()
	{
		std::string_view typeName = typeid(T).name();
		size_t pos = typeName.find_last_of(':');
		std::string_view structName = typeName.substr(pos + 1);
		std::string managedTypeName = fmt::format("Eppo.{}", structName);

		MonoType* managedType = mono_reflection_type_from_name(managedTypeName.data(), ScriptEngine::GetCoreAssemblyImage());
		if (!managedType)
		{
			EPPO_ERROR("Could not find component type {}", managedTypeName);
			return;
		}

		auto fn = [](Entity entity) { return entity.HasComponent<T>(); };
		s_EntityHasComponentFns.insert_or_assign(managedType, fn);
	}
}