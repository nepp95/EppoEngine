#stage vert
#version 450

#include "Includes/base.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = inColor;

	gl_Position = uCamera.ViewProjection * vec4(inPosition, 1.0);
}

#stage frag
#version 450

layout(location = 0) in vec4 inColor;

layout(location = 0) out vec4 outFragColor;

void main()
{
	outFragColor = inColor;
}
