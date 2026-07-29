#pragma once

#include "Core/UUID.h"
#include "Scripting/ScriptField.h"

namespace Eppo
{
    using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldValue>;
    using ScriptFieldStorage = std::unordered_map<UUID, ScriptFieldMap>;
}
