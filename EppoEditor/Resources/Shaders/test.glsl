#stage vert
#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

void main()
{
	gl_Position = vec4(inPosition, 1.0);
}

#stage frag
#version 450 core

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vec4(1.0);
}
