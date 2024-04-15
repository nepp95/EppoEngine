#include "pch.h"
#include "ScriptInstance.h"

#include "Scripting/ScriptEngine.h"

#include <mono/jit/jit.h>

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

	bool ScriptInstance::GetFieldValueInternal(const std::string& name, void* buffer)
	{
		const auto& fields = m_ScriptClass->GetFields();
		auto it = fields.find(name);
		if (it == fields.end())
			return false;

		const ScriptField& field = it->second;
		mono_field_get_value(m_Instance, field.ClassField, buffer);

		return true;
	}

	bool ScriptInstance::SetFieldValueInternal(const std::string& name, const void* value)
	{
		const auto& fields = m_ScriptClass->GetFields();
		auto it = fields.find(name);
		if (it == fields.end())
			return false;

		const ScriptField& field = it->second;
		mono_field_set_value(m_Instance, field.ClassField, (void*)value);

		return true;
	}
}