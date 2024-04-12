#pragma once

#include <mono/metadata/object.h>

namespace Eppo
{
	static void Log(uint32_t logLevel, MonoString* message)
	{
		char* cStr = mono_string_to_utf8(message);
		std::string messageStr(cStr);
		mono_free(cStr);

		switch (logLevel)
		{
			case 0: EPPO_TRACE(messageStr); break;
			case 1: EPPO_INFO(messageStr); break;
			case 2: EPPO_WARN(messageStr); break;
			case 3: EPPO_ERROR(messageStr); break;
		}
	}
}
