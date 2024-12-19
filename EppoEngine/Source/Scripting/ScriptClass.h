#pragma once

#include "Scripting/ScriptField.h"

typedef struct _MonoClass MonoClass;
typedef struct _MonoMethod MonoMethod;
typedef struct _MonoObject MonoObject;

namespace Eppo
{
	class ScriptClass
	{
	public:
		ScriptClass() = default;
		ScriptClass(std::string nameSpace, std::string name, bool isCore = false);
		explicit ScriptClass(MonoClass* monoClass);

		[[nodiscard]] MonoObject* Instantiate() const;
		[[nodiscard]] MonoMethod* GetMethod(const std::string& name, uint32_t paramCount) const;
		MonoObject* InvokeMethod(MonoObject* instance, MonoMethod* method, void** params = nullptr);

		[[nodiscard]] const std::unordered_map<std::string, ScriptField>& GetFields() const { return m_Fields; }

	private:
		std::string m_Namespace;
		std::string m_Name;

		std::unordered_map<std::string, ScriptField> m_Fields;

		MonoClass* m_MonoClass = nullptr;

		friend class ScriptEngine;
	};
}
