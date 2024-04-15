#include "pch.h"
#include "ScriptEngine.h"

#include "Core/Buffer.h"
#include "Core/Filesystem.h"
#include "Scripting/ScriptGlue.h"
#include "Scripting/ScriptInstance.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/tabledefs.h>
#include <mono/metadata/threads.h>

namespace Eppo
{
	static std::unordered_map<std::string, ScriptFieldType> s_ScriptFieldTypeMap {
		{ "System.Single",		ScriptFieldType::Float },
		{ "System.Double",		ScriptFieldType::Double },
		{ "System.Boolean",		ScriptFieldType::Bool },
		{ "System.Char",		ScriptFieldType::Char },
		{ "System.Int16",		ScriptFieldType::Int16 },
		{ "System.Int32",		ScriptFieldType::Int32 },
		{ "System.Int64",		ScriptFieldType::Int64 },
		{ "System.Byte",		ScriptFieldType::Byte },
		{ "System.UInt16",		ScriptFieldType::UInt16 },
		{ "System.UInt32",		ScriptFieldType::UInt32 },
		{ "System.UInt64",		ScriptFieldType::UInt64 },

		{ "Eppo.Vector2",		ScriptFieldType::Vector2 },
		{ "Eppo.Vector3",		ScriptFieldType::Vector3 },
		{ "Eppo.Vector4",		ScriptFieldType::Vector4 },

		{ "Eppo.Entity",		ScriptFieldType::Entity },
	};

	namespace Utils
	{
		static MonoAssembly* LoadMonoAssembly(const std::filesystem::path& filepath, bool loadPDB = false)
		{
			ScopedBuffer buffer = Filesystem::ReadBytes(filepath);

			MonoImageOpenStatus status;
			MonoImage* image = mono_image_open_from_data_full(buffer.As<char>(), buffer.Size(), 1, &status, 0);

			if (status != MONO_IMAGE_OK)
			{
				const char* error = mono_image_strerror(status);
				EPPO_ERROR(error);
				return false;
			}

			if (loadPDB)
			{
				std::filesystem::path pdbPath = filepath;
				pdbPath.replace_extension(".pdb");

				if (Filesystem::Exists(pdbPath))
				{
					ScopedBuffer pdbBuffer = Filesystem::ReadBytes(pdbPath);
					mono_debug_open_image_from_memory(image, pdbBuffer.As<const mono_byte>(), pdbBuffer.Size());
					EPPO_INFO("Loaded PDB: {}", pdbPath);
				}
			}

			MonoAssembly* assembly = mono_assembly_load_from_full(image, filepath.string().c_str(), &status, 0);
			mono_image_close(image);

			return assembly;
		}

		static ScriptFieldType MonoTypeToScriptFieldType(MonoType* monoType)
		{
			std::string type = mono_type_get_name(monoType);

			auto it = s_ScriptFieldTypeMap.find(type);
			if (it != s_ScriptFieldTypeMap.end())
				return it->second;

			return ScriptFieldType::None;
		}
	}

	struct ScriptEngineData
	{
		MonoDomain* RootDomain = nullptr;
		MonoDomain* AppDomain = nullptr;

		MonoAssembly* CoreAssembly = nullptr;
		MonoImage* CoreAssemblyImage = nullptr;
		std::filesystem::path CoreAssemblyFilepath;

		MonoAssembly* AppAssembly = nullptr;
		MonoImage* AppAssemblyImage = nullptr;
		std::filesystem::path AppAssemblyFilepath;

		Ref<ScriptClass> EntityClass;

		std::unordered_map<std::string, Ref<ScriptClass>> EntityScriptClasses;
		std::unordered_map<UUID, Ref<ScriptInstance>> EntityScriptInstances;
		std::unordered_map<UUID, ScriptFieldMap> EntityScriptFields;

		Scene* SceneContext;

		#if defined(EPPO_DEBUG)
			bool EnableDebugging = true;
		#else
			bool EnableDebugging = false;
		#endif
	};

	static ScriptEngineData* s_Data;

	void ScriptEngine::Init()
	{
		s_Data = new ScriptEngineData();

		InitMono();

		bool status = LoadCoreAssembly("Resources/Scripts/EppoScripting.dll");
		if (!status)
			EPPO_ERROR("Failed to load EppoScripting assembly!");

		LoadAppAssembly("Projects/Assets/Scripts/Binaries/Sandbox.dll");
	}

	void ScriptEngine::Shutdown()
	{
		mono_domain_set(mono_get_root_domain(), false);

		mono_domain_unload(s_Data->AppDomain);
		s_Data->AppDomain = nullptr;

		mono_jit_cleanup(s_Data->RootDomain);
		s_Data->RootDomain = nullptr;

		delete s_Data;
	}

	bool ScriptEngine::LoadAppAssembly(const std::filesystem::path& filepath)
	{
		// Load assembly
		s_Data->AppAssemblyFilepath = filepath;
		s_Data->AppAssembly = Utils::LoadMonoAssembly(filepath, s_Data->EnableDebugging);
		if (s_Data->AppAssembly == nullptr)
			return false;

		s_Data->AppAssemblyImage = mono_assembly_get_image(s_Data->AppAssembly);

		// Register internal calls
		ScriptGlue::RegisterFunctions();
		ScriptGlue::RegisterComponents();

		// Load assembly classes
		LoadAssemblyClasses();

		// Create base entity class
		s_Data->EntityClass = CreateRef<ScriptClass>("Eppo", "Entity", true);

		return true;
	}

	void ScriptEngine::OnRuntimeStart(Scene* scene)
	{
		s_Data->SceneContext = scene;
	}

	void ScriptEngine::OnRuntimeStop()
	{
		s_Data->SceneContext = nullptr;
		s_Data->EntityScriptInstances.clear();
	}

	void ScriptEngine::OnCreateEntity(Entity entity)
	{
		const auto& sc = entity.GetComponent<ScriptComponent>();
		if (EntityClassExists(sc.ClassName))
		{
			UUID uuid = entity.GetUUID();

			Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(s_Data->EntityScriptClasses.at(sc.ClassName), entity);
			s_Data->EntityScriptInstances[uuid] = instance;

			auto it = s_Data->EntityScriptFields.find(uuid);
			if (it != s_Data->EntityScriptFields.end())
			{
				const ScriptFieldMap& fieldMap = it->second;
				for (const auto& [name, fieldInstance] : fieldMap)
				{
					instance->SetFieldValue(name, fieldInstance.m_Buffer);
				}
			}

			instance->InvokeOnCreate();
		}
	}

	void ScriptEngine::OnUpdateEntity(Entity entity, float timestep)
	{
		UUID uuid = entity.GetUUID();
		EPPO_ASSERT(s_Data->EntityScriptInstances.find(uuid) != s_Data->EntityScriptInstances.end());

		Ref<ScriptInstance> instance = s_Data->EntityScriptInstances.at(uuid);
		instance->InvokeOnUpdate(timestep);
	}

	MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass)
	{
		MonoObject* instance = mono_object_new(s_Data->AppDomain, monoClass);
		mono_runtime_object_init(instance);

		return instance;
	}

	bool ScriptEngine::EntityClassExists(const std::string& fullName)
	{
		return s_Data->EntityScriptClasses.find(fullName) != s_Data->EntityScriptClasses.end();
	}

	MonoImage* ScriptEngine::GetCoreAssemblyImage()
	{
		return s_Data->CoreAssemblyImage;
	}

	MonoImage* ScriptEngine::GetAppAssemblyImage()
	{
		return s_Data->AppAssemblyImage;
	}

	Scene* ScriptEngine::GetSceneContext()
	{
		return s_Data->SceneContext;
	}

	Ref<ScriptClass> ScriptEngine::GetEntityClass()
	{
		return s_Data->EntityClass;
	}

	Ref<ScriptClass> ScriptEngine::GetEntityClass(const std::string& name)
	{
		auto it = s_Data->EntityScriptClasses.find(name);
		if (it == s_Data->EntityScriptClasses.end())
			return nullptr;

		return it->second;
	}

	Ref<ScriptInstance> ScriptEngine::GetEntityInstance(UUID uuid)
	{
		auto it = s_Data->EntityScriptInstances.find(uuid);
		EPPO_ASSERT(it != s_Data->EntityScriptInstances.end());

		return it->second;
	}

	ScriptFieldMap& ScriptEngine::GetScriptFieldMap(UUID uuid)
	{
		return s_Data->EntityScriptFields[uuid];
	}

	std::unordered_map<std::string, Ref<ScriptClass>>& ScriptEngine::GetEntityClasses()
	{
		return s_Data->EntityScriptClasses;
	}

	void ScriptEngine::InitMono()
	{
		mono_set_assemblies_path("Mono/lib");

		s_Data->RootDomain = mono_jit_init("EppoJITRuntime");
		EPPO_ASSERT(s_Data->RootDomain);

		if (s_Data->EnableDebugging)
			mono_debug_domain_create(s_Data->RootDomain);

		mono_thread_set_main(mono_thread_current());
	}

	bool ScriptEngine::LoadCoreAssembly(const std::filesystem::path& filepath)
	{
		// Setup appdomain
		s_Data->AppDomain = mono_domain_create_appdomain("EppoScriptRuntime", nullptr);
		mono_domain_set(s_Data->AppDomain, true);

		// Setup core assembly
		s_Data->CoreAssemblyFilepath = filepath;
		s_Data->CoreAssembly = Utils::LoadMonoAssembly(filepath, s_Data->EnableDebugging);
		if (s_Data->CoreAssembly == nullptr)
			return false;

		s_Data->CoreAssemblyImage = mono_assembly_get_image(s_Data->CoreAssembly);

		return true;
	}

	void ScriptEngine::LoadAssemblyClasses()
	{
		const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(s_Data->AppAssemblyImage, MONO_TABLE_TYPEDEF);
		int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);
		MonoClass* entityClass = mono_class_from_name(s_Data->CoreAssemblyImage, "Eppo", "Entity");

		for (int32_t i = 0; i < numTypes; i++)
		{
			// Get namespace and class name from image
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

			const char* nameSpace = mono_metadata_string_heap(s_Data->AppAssemblyImage, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* name = mono_metadata_string_heap(s_Data->AppAssemblyImage, cols[MONO_TYPEDEF_NAME]);

			std::string fullName;
			if (strlen(nameSpace) != 0)
				fullName = fmt::format("{}.{}", nameSpace, name);
			else
				fullName = name;

			// TodO: Filter out module?

			// Get mono class handle from name
			MonoClass* monoClass = mono_class_from_name(s_Data->AppAssemblyImage, nameSpace, name);

			// We're looking for all classes EXCEPT the entity class
			if (monoClass == entityClass)
				continue;

			bool isEntity = mono_class_is_subclass_of(monoClass, entityClass, false);
			if (!isEntity)
				continue;

			// Create a reference to the class and process it's fields
			Ref<ScriptClass> scriptClass = CreateRef<ScriptClass>(monoClass);
			s_Data->EntityScriptClasses.insert_or_assign(fullName, scriptClass);

			uint32_t numFields = mono_class_num_fields(monoClass);

			void* gPointer = nullptr;
			uint32_t publicFields = 0;
			while (MonoClassField* field = mono_class_get_fields(monoClass, &gPointer))
			{
				uint32_t flags = mono_field_get_flags(field);
				if (flags & FIELD_ATTRIBUTE_PUBLIC)
				{
					publicFields++;

					MonoType* fieldType = mono_field_get_type(field);
					std::string fieldName = mono_field_get_name(field);

					ScriptFieldType scriptFieldType = Utils::MonoTypeToScriptFieldType(fieldType);
					if (scriptFieldType == ScriptFieldType::None)
						continue;

					ScriptField scriptField;
					scriptField.Type = scriptFieldType;
					scriptField.Name = fieldName;
					scriptField.ClassField = field;

					EPPO_TRACE("{}::{}", Utils::ScriptFieldTypeToString(scriptField.Type), scriptField.Name);

					scriptClass->m_Fields.insert_or_assign(fieldName, scriptField);
				}
			}

			EPPO_TRACE("{} has {} fields of which {} are public: ", fullName, numFields, publicFields);
		}
	}
}