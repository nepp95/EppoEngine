#pragma once

#include <glad/glad.h>

namespace Eppo
{
	// https://www.khronos.org/opengl/wiki/Debug_Output
	void DebugCallback(unsigned source, unsigned type, unsigned id, unsigned severity, int length, const char* message, const void* userParam)
	{
		switch (severity)
		{
			case GL_DEBUG_SEVERITY_HIGH:
			case GL_DEBUG_SEVERITY_MEDIUM:
			case GL_DEBUG_SEVERITY_LOW:
			{
				EPPO_ERROR(message);
				break;
			}

			case GL_DEBUG_SEVERITY_NOTIFICATION:
			{
				EPPO_WARN(message);
				break;
			}
		}
	}
}
