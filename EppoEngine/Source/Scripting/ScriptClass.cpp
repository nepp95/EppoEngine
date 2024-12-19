#include "pch.h"
#include "ScriptClass.h"

#include "Scripting/ScriptEngine.h"

#include <mono/metadata/object.h>

namespace Eppo
{
	ScriptClass::ScriptClass(std::string nameSpace, std::string name, const bool isCore)
		: m_Namespace(std::move(nameSpace)), m_Name(std::move(name))
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

	MonoMethod* ScriptClass::GetMethod(const std::string& name, const uint32_t paramCount) const
	{
		EPPO_PROFILE_FUNCTION("ScriptClass::GetMethod");

		return mono_class_get_method_from_name(m_MonoClass, name.c_str(), static_cast<int>(paramCount));
	}

	MonoObject* ScriptClass::InvokeMethod(MonoObject* instance, MonoMethod* method, void** params)
	{
		EPPO_PROFILE_FUNCTION("ScriptClass::InvokeMethod");

		MonoObject* exception = nullptr;
		return mono_runtime_invoke(method, instance, params, &exception);
	}
}
