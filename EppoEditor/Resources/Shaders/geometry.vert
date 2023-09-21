#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(set = 1, binding = 0) uniform Camera
{
	mat4 uViewProjection;
};

layout(push_constant) uniform Transform
{
	mat4 Transform;
} uTransform;

void main()
{
	gl_Position = uViewProjection * uTransform.Transform * vec4(inPosition, 1.0);
}
