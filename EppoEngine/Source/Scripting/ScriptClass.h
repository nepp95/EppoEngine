#pragma once

#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptField.h"

namespace Eppo
{
	class ScriptClass
	{
	public:
		ScriptClass() = default;
		ScriptClass(const std::string& nameSpace, const std::string& name, bool isCore = false);
		ScriptClass(MonoClass* monoClass);

		MonoObject* Instantiate() const;
		MonoMethod* GetMethod(const std::string& name, uint32_t paramCount);
		MonoObject* InvokeMethod(MonoObject* instance, MonoMethod* method, void** params = nullptr);

		const std::unordered_map<std::string, ScriptField>& GetFields() const { return m_Fields; }

	private:
		std::string m_Namespace;
		std::string m_Name;

		std::unordered_map<std::string, ScriptField> m_Fields;

		MonoClass* m_MonoClass = nullptr;

		friend class ScriptEngine;
	};
}
