#include "pch.h"
#include "ScriptClass.h"

#include "Scripting/ScriptEngine.h"

#include <mono/metadata/object.h>

namespace Eppo
{
	ScriptClass::ScriptClass(const std::string& nameSpace, const std::string& name, bool isCore)
		: m_Namespace(nameSpace), m_Name(name)
	{
		EPPO_PROFILE_FUNCTION("ScriptClass::ScriptClass");

		if (isCore)
			m_MonoClass = mono_class_from_name(ScriptEngine::GetCoreAssemblyImage(), m_Namespace.c_str(), m_Name.c_str());
		else
			m_MonoClass = mono_class_from_name(ScriptEngine::GetAppAssemblyImage(), m_Namespace.c_str(), m_Name.c_str());
	}

	ScriptClass::ScriptClass(MonoClass* monoClass)
		: m_MonoClass(monoClass)
	{
		EPPO_PROFILE_FUNCTION("ScriptClass::ScriptClass");

		m_Namespace = mono_class_get_namespace(m_MonoClass);
		m_Name = mono_class_get_name(m_MonoClass);
	}

	MonoObject* ScriptClass::Instantiate() const
	{
		EPPO_PROFILE_FUNCTION("ScriptClass::Instantiate");

		return ScriptEngine::InstantiateClass(m_MonoClass);
	}

	MonoMethod* ScriptClass::GetMethod(const std::string& name, uint32_t paramCount)
	{
		EPPO_PROFILE_FUNCTION("ScriptClass::GetMethod");

		return mono_class_get_method_from_name(m_MonoClass, name.c_str(), paramCount);
	}

	MonoObject* ScriptClass::InvokeMethod(MonoObject* instance, MonoMethod* method, void** params)
	{
		EPPO_PROFILE_FUNCTION("ScriptClass::InvokeMethod");

		MonoObject* exception = nullptr;
		return mono_runtime_invoke(method, instance, params, &exception);
	}
}
