#pragma once

#include "Scene/Entity.h"
#include "Scripting/ScriptClass.h"

typedef struct _MonoMethod MonoMethod;
typedef struct _MonoObject MonoObject;

namespace Eppo
{
	class ScriptInstance
	{
	public:
		ScriptInstance(const Ref<ScriptClass>& scriptClass, Entity entity);

		void InvokeOnCreate() const;
		void InvokeOnUpdate(float timestep) const;

		Ref<ScriptClass> GetScriptClass() { return m_ScriptClass; }
		[[nodiscard]] MonoObject* GetManagedObject() const { return m_Instance; }

		template<typename T>
		T GetFieldValue(const std::string& name)
		{
			static_assert(sizeof(T) <= 16, "Type too large!");

			if (!GetFieldValueInternal(name, s_FieldValueBuffer))
				return T();

			return *(T*)s_FieldValueBuffer;
		}

		template<typename T>
		void SetFieldValue(const std::string& name, const T& value)
		{
			static_assert(sizeof(T) <= 16, "Type too large!");
			SetFieldValueInternal(name, &value);
		}

	private:
		bool GetFieldValueInternal(const std::string& name, void* buffer) const;
		bool SetFieldValueInternal(const std::string& name, const void* value) const;

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
