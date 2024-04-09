#pragma once

#include "Scripting/InternalCalls/Log.h"

#include <mono/metadata/reflection.h>

namespace Eppo
{
	#define EPPO_ADD_INTERNAL_CALL(fn) mono_add_internal_call("Eppo.InternalCalls::"#fn, fn);

	class ScriptGlue
	{
	public:
		static void RegisterFunctions()
		{
			EPPO_ADD_INTERNAL_CALL(Log);
		}
	};
}
