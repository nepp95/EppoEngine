#include "pch.h"
#include "ScriptInstance.h"

#include "Scripting/ScriptEngine.h"

namespace Eppo
{
	ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity)
		: m_ScriptClass(scriptClass)
	{
		m_Instance = m_ScriptClass->Instantiate();

		m_Constructor = ScriptEngine::GetEntityClass()->GetMethod(".ctor", 1);
		m_OnCreate = m_ScriptClass->GetMethod("OnCreate", 0);
		m_OnUpdate = m_ScriptClass->GetMethod("OnUpdate", 1);

		UUID uuid = entity.GetUUID();
		void* param = &uuid;
		m_ScriptClass->InvokeMethod(m_Instance, m_Constructor, &param);
	}

	void ScriptInstance::InvokeOnCreate()
	{
		if (m_OnCreate)
			m_ScriptClass->InvokeMethod(m_Instance, m_OnCreate);
	}

	void ScriptInstance::InvokeOnUpdate(float timestep)
	{
		if (m_OnUpdate)
		{
			void* params = &timestep;
			m_ScriptClass->InvokeMethod(m_Instance, m_OnUpdate, &params);
		}
	}

	template<typename T>
	void ScriptInstance::SetFieldValue(const std::string& name, const T& value)
	{
		static_assert(sizeof(T) <= 16, "Type too large!");

		const auto& fields = m_ScriptClass->GetFields();
		auto it = fields.find(name);
		if (it == fields.end())
			return;

		const ScriptField& field = it->second;
		mono_field_set_value(m_Instance, field.ClassField, (void*)&value); // might be cause of issue
	}

	template<typename T>
	T ScriptInstance::GetFieldValue(const std::string& name)
	{
		static_assert(sizeof(T) <= 16, "Type too large!");

		const auto& fields = m_ScriptClass->GetFields();
		auto it = fields.find(name);
		if (it == fields.end())
			return T();

		const ScriptField& field = it->second;
		mono_field_get_value(m_Instance, field.ClassField, s_FieldValueBuffer);

		return *(T*)s_FieldValueBuffer;
	}
}
