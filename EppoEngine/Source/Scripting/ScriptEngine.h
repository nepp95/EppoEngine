#pragma once

#include "Scene/Scene.h"
#include "Scripting/ScriptClass.h"
#include "Scripting/ScriptField.h"
#include "Scripting/ScriptInstance.h"

typedef struct _MonoAssembly MonoAssembly;
typedef struct _MonoClass MonoClass;
typedef struct _MonoDomain MonoDomain;
typedef struct _MonoImage MonoImage;
typedef struct _MonoMethod MonoMethod;
typedef struct _MonoObject MonoObject;

namespace Eppo
{
	using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldInstance>;

	class ScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static bool LoadAppAssembly(const std::filesystem::path& filepath);

		static void OnRuntimeStart(Scene* scene);
		static void OnRuntimeStop();
		static void OnCreateEntity(Entity entity);
		static void OnUpdateEntity(Entity entity, float timestep);

		static MonoObject* InstantiateClass(MonoClass* monoClass);
		static bool EntityClassExists(const std::string& fullName);

		static MonoImage* GetCoreAssemblyImage();
		static MonoImage* GetAppAssemblyImage();
		static Scene* GetSceneContext();

		static Ref<ScriptClass> GetEntityClass();
		static Ref<ScriptClass> GetEntityClass(const std::string& name);
		static Ref<ScriptInstance> GetEntityInstance(UUID uuid);
		static ScriptFieldMap& GetScriptFieldMap(UUID uuid);
		static std::unordered_map<std::string, Ref<ScriptClass>>& GetEntityClasses();
		
		static MonoObject* GetManagedInstance(UUID uuid);

	private:
		static void InitMono();
		static bool LoadCoreAssembly(const std::filesystem::path& filepath);
		static void LoadAssemblyClasses();
	};

	namespace Utils
	{
		inline ScriptFieldType ScriptFieldTypeFromString(std::string_view typeString)
		{
			if (typeString == "Float")			return ScriptFieldType::Float;
			if (typeString == "Double")			return ScriptFieldType::Double;
			if (typeString == "Bool")			return ScriptFieldType::Bool;
			if (typeString == "Char")			return ScriptFieldType::Char;
			if (typeString == "Int16")			return ScriptFieldType::Int16;
			if (typeString == "Int32")			return ScriptFieldType::Int32;
			if (typeString == "Int64")			return ScriptFieldType::Int64;
			if (typeString == "Byte")			return ScriptFieldType::Byte;
			if (typeString == "UInt16")			return ScriptFieldType::UInt16;
			if (typeString == "UInt32")			return ScriptFieldType::UInt32;
			if (typeString == "UInt64")			return ScriptFieldType::UInt64;
			if (typeString == "Vector2")		return ScriptFieldType::Vector2;
			if (typeString == "Vector3")		return ScriptFieldType::Vector3;
			if (typeString == "Vector4")		return ScriptFieldType::Vector4;
			if (typeString == "Entity")			return ScriptFieldType::Entity;

			EPPO_ASSERT(false);
			return ScriptFieldType::None;
		}

		inline std::string ScriptFieldTypeToString(ScriptFieldType type)
		{
			switch (type)
			{
				case ScriptFieldType::Float:	return "Float";
				case ScriptFieldType::Double:	return "Double";
				case ScriptFieldType::Bool:		return "Bool";
				case ScriptFieldType::Char:		return "Char";
				case ScriptFieldType::Int16:	return "Int16";
				case ScriptFieldType::Int32:	return "Int32";
				case ScriptFieldType::Int64:	return "Int64";
				case ScriptFieldType::Byte:		return "Byte";
				case ScriptFieldType::UInt16:	return "UInt16";
				case ScriptFieldType::UInt32:	return "UInt32";
				case ScriptFieldType::UInt64:	return "UInt64";
				case ScriptFieldType::Vector2:	return "Vector2";
				case ScriptFieldType::Vector3:	return "Vector3";
				case ScriptFieldType::Vector4:	return "Vector4";
				case ScriptFieldType::Entity:	return "Entity";
			}

			EPPO_ASSERT(false);
			return "None";
		}
	}
}
