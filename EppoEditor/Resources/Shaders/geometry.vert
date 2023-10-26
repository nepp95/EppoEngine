#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outNormal;

layout(set = 1, binding = 0) uniform Camera
{
	mat4 uViewProjection;
};

layout(push_constant) uniform Transform
{
	mat4 uTransform;
};

void main()
{
	outNormal = inNormal;

	gl_Position = uViewProjection * uTransform * vec4(inPosition, 1.0);
}
