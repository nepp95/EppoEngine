#pragma once

#include "Scene/Entity.h"
#include "Scripting/ScriptClass.h"

namespace Eppo
{
	class ScriptInstance
	{
	public:
		ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity);

		void InvokeOnCreate();
		void InvokeOnUpdate(float timestep);

		Ref<ScriptClass> GetScriptClass() { return m_ScriptClass; }
		MonoObject* GetManagedObject() { return m_Instance; }

		template<typename T>
		T GetFieldValue(const std::string& name);

		template<typename T>
		void SetFieldValue(const std::string& name, const T& value);

	private:
		Ref<ScriptClass> m_ScriptClass;
		MonoObject* m_Instance = nullptr;
		MonoMethod* m_Constructor = nullptr;
		MonoMethod* m_OnCreate = nullptr;
		MonoMethod* m_OnUpdate = nullptr;

		inline static char s_FieldValueBuffer[16];

		friend class ScriptEngine;
		friend class ScriptFieldInstance;
	};
}
