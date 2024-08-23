#include "pch.h"
#include "ScriptGlue.h"

#include "Asset/AssetManager.h"
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
		EPPO_PROFILE_FUNCTION("ScriptGlue::Log");

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
		EPPO_PROFILE_FUNCTION("ScriptGlue::Input_IsKeyPressed");

		return Input::IsKeyPressed(keyCode);
	}

	static void Entity_AddComponent(UUID uuid, MonoString* componentType)
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::Entity_AddComponent");

		Ref<Scene> scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		char* cStr = mono_string_to_utf8(componentType);
		std::string componentTypeStr(cStr);
		mono_free(cStr);

		if (componentTypeStr == "TransformComponent")
			entity.AddComponent<TransformComponent>();
		if (componentTypeStr == "SpriteComponent")
			entity.AddComponent<SpriteComponent>();
		if (componentTypeStr == "MeshComponent")
			entity.AddComponent<MeshComponent>();
		if (componentTypeStr == "DirectionalLightComponent")
			entity.AddComponent<DirectionalLightComponent>();
		if (componentTypeStr == "ScriptComponent")
			entity.AddComponent<ScriptComponent>();
		if (componentTypeStr == "RigidBodyComponent")
			entity.AddComponent<RigidBodyComponent>();
	}

	static uint64_t Entity_CreateNewEntity(MonoString* name)
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::Entity_CreateNewEntity");

		Ref<Scene> scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);

		char* cStr = mono_string_to_utf8(name);
		std::string nameStr(cStr);
		mono_free(cStr);

		Entity entity = scene->CreateEntity(nameStr);
		EPPO_ASSERT(entity);

		return entity.GetUUID();
	}

	static uint64_t Entity_FindEntityByName(MonoString* name)
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::Entity_FindEntityByName");

		Ref<Scene> scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);

		char* cStr = mono_string_to_utf8(name);
		std::string nameStr(cStr);
		mono_free(cStr);

		Entity entity = scene->FindEntityByName(nameStr);
		if (!entity)
			return 0;

		return entity.GetUUID();
	}
	
	static MonoString* Entity_GetName(UUID uuid)
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::Entity_GetName");

		Ref<Scene> scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		MonoString* monoStr = mono_string_new(ScriptEngine::GetAppDomain(), entity.GetName().c_str());

		return monoStr;
	}

	static bool Entity_HasComponent(UUID uuid, MonoReflectionType* componentType)
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::Entity_HasComponent");

		Ref<Scene> scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		MonoType* managedType = mono_reflection_type_get_type(componentType);
		auto it = s_EntityHasComponentFns.find(managedType);
		EPPO_ASSERT(it != s_EntityHasComponentFns.end());
		return it->second(entity);
	}

	static MonoString* MeshComponent_GetMeshFilepath(UUID uuid)
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::MeshComponent_GetMeshFilepath");

		Ref<Scene> scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		AssetHandle assetHandle = entity.GetComponent<MeshComponent>().MeshHandle;
		AssetMetadata& metadata = AssetManager::Get().GetMetadata(assetHandle);

		MonoString* monoStr = mono_string_new(ScriptEngine::GetAppDomain(), metadata.Filepath.string().c_str());

		return monoStr;
	}

	static void MeshComponent_SetMesh(UUID uuid, MonoString* filepath)
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::MeshComponent_SetMesh");

		Ref<Scene> scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		char* cStr = mono_string_to_utf8(filepath);
		std::string filepathStr(cStr);
		mono_free(cStr);

		EPPO_ASSERT(AssetManager::Get().IsAssetLoaded(filepathStr));
		AssetMetadata& metadata = AssetManager::Get().GetMetadata(filepathStr);
		
		auto& mc = entity.GetComponent<MeshComponent>();
		mc.MeshHandle = metadata.Handle;
	}

	static void TransformComponent_GetTranslation(UUID uuid, glm::vec3* outTranslation)
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::TransformComponent_GetTranslation");

		Ref<Scene> scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		*outTranslation = entity.GetComponent<TransformComponent>().Translation;
	}

	static void TransformComponent_SetTranslation(UUID uuid, glm::vec3* translation)
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::TransformComponent_SetTranslation");

		Ref<Scene> scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		entity.GetComponent<TransformComponent>().Translation = *translation;
	}

	static void RigidBodyComponent_ApplyLinearImpulse(UUID uuid, glm::vec3* impulse, glm::vec3* worldPosition)
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::RigidBodyComponent_ApplyLinearImpulse");

		Ref<Scene> scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		auto& rb = entity.GetComponent<RigidBodyComponent>();
		rb.RuntimeBody.ApplyLinearImpulse(*impulse, *worldPosition);
	}

	static void RigidBodyComponent_ApplyLinearImpulseToCenter(UUID uuid, glm::vec3* impulse)
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::RigidBodyComponent_ApplyLinearImpulseToCenter");

		Ref<Scene> scene = ScriptEngine::GetSceneContext();
		EPPO_ASSERT(scene);
		Entity entity = scene->FindEntityByUUID(uuid);
		EPPO_ASSERT(entity);

		auto& rb = entity.GetComponent<RigidBodyComponent>();
		rb.RuntimeBody.ApplyLinearImpulse(*impulse);
	}

	static MonoObject* GetScriptInstance(UUID uuid)
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::GetScriptInstance");

		return ScriptEngine::GetManagedInstance(uuid);
	}

	void ScriptGlue::RegisterFunctions()
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::RegisterFunctions");

		EPPO_ADD_INTERNAL_CALL(Log);
		EPPO_ADD_INTERNAL_CALL(Input_IsKeyPressed);
		EPPO_ADD_INTERNAL_CALL(Entity_AddComponent);
		EPPO_ADD_INTERNAL_CALL(Entity_CreateNewEntity);
		EPPO_ADD_INTERNAL_CALL(Entity_FindEntityByName);
		EPPO_ADD_INTERNAL_CALL(Entity_GetName);
		EPPO_ADD_INTERNAL_CALL(Entity_HasComponent);
		EPPO_ADD_INTERNAL_CALL(MeshComponent_GetMeshFilepath);
		EPPO_ADD_INTERNAL_CALL(MeshComponent_SetMesh);
		EPPO_ADD_INTERNAL_CALL(TransformComponent_GetTranslation);
		EPPO_ADD_INTERNAL_CALL(TransformComponent_SetTranslation);
		EPPO_ADD_INTERNAL_CALL(RigidBodyComponent_ApplyLinearImpulse);
		EPPO_ADD_INTERNAL_CALL(RigidBodyComponent_ApplyLinearImpulseToCenter);
		EPPO_ADD_INTERNAL_CALL(GetScriptInstance);
	}

	void ScriptGlue::RegisterComponents()
	{
		EPPO_PROFILE_FUNCTION("ScriptGlue::RegisterComponents");

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
		EPPO_PROFILE_FUNCTION("ScriptGlue::RegisterComponent");

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
