#pragma once

#include "Scene/Entity.h"
#include "Scripting/ScriptField.h"

namespace Eppo
{
    class ScriptClass
    {
    public:
        ScriptClass() = default;
        ScriptClass(std::string fullName, std::string nameSpace, std::string name, bool isCore = false);

        [[nodiscard]] auto GetFullName() const -> const std::string& { return m_FullName; }
        [[nodiscard]] auto GetNamespace() const -> const std::string& { return m_Namespace; }
        [[nodiscard]] auto GetName() const -> const std::string& { return m_Name; }
        [[nodiscard]] auto IsCore() const -> bool { return m_IsCore; }
        [[nodiscard]] auto GetFields() const -> const std::vector<ScriptField>& { return m_Fields; }
        [[nodiscard]] auto GetMethods() const -> const std::vector<ScriptMethod>& { return m_Methods; }
        [[nodiscard]] auto GetMethod(const std::string& name) const -> const ScriptMethod*;
        auto InvokeMethod(Entity entity, const ScriptMethod& method, const void* args = nullptr, void* ret = nullptr) const -> void;

    private:
        std::string m_FullName;
        std::string m_Namespace;
        std::string m_Name;
        bool m_IsCore;

        std::vector<ScriptField> m_Fields;
        std::vector<ScriptMethod> m_Methods;

        friend class Assembly;
    };
}