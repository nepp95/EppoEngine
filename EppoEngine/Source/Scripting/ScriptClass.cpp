#include "pch.h"
#include "Scripting/ScriptClass.h"

#include "Scripting/ScriptEngine.h"

namespace Eppo
{
    ScriptClass::ScriptClass(std::string fullName, std::string nameSpace, std::string name, const bool isCore)
        : m_FullName(std::move(fullName)), m_Namespace(std::move(nameSpace)), m_Name(std::move(name)), m_IsCore(isCore)
    {}

    auto ScriptClass::GetMethod(const std::string& name) const -> const ScriptMethod*
    {
        for (const auto& method : m_Methods)
            if (method.Name == name)
                return &method;
        return nullptr;
    }

    auto ScriptClass::InvokeMethod(const Entity entity, const ScriptMethod& method, const void* args, void* ret) const -> void
    {
        ScriptEngine::Get().InvokeMethod(entity, method, args, ret);
    }
}
