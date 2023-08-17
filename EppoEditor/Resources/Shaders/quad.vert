#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in float inTexIndex;

layout(location = 0) out vec4 fColor;
layout(location = 1) out vec2 fTexCoord;
layout(location = 2) out flat float fTexIndex;

layout(set = 1, binding = 0) uniform Camera
{
	mat4 uViewProjection;
};

void main()
{
	fColor = inColor;
	fTexCoord = inTexCoord;
	fTexIndex = inTexIndex;

	gl_Position = uViewProjection * vec4(inPosition, 1.0);
}
