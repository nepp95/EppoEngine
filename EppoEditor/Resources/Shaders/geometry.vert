#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out flat float outColorID;

layout(set = 1, binding = 0) uniform Camera
{
	mat4 uViewProjection;
};

void main()
{
	outColorID = gl_VertexIndex;
	gl_Position = uViewProjection * vec4(inPosition, 1.0);
}
