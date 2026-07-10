#include "pch.h"
#include "Scripting/ScriptClass.h"

namespace Eppo
{
    ScriptClass::ScriptClass(std::string fullName, std::string nameSpace, std::string name, const bool isCore)
        : m_FullName(std::move(fullName)), m_Namespace(std::move(nameSpace)), m_Name(std::move(name)), m_IsCore(isCore)
    {}
}
