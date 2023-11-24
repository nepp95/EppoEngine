#stage vert
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
	layout(offset = 0) mat4 uTransform;
};

void main()
{
	gl_Position = uViewProjection * uTransform * vec4(inPosition, 1.0);
}

#stage frag
#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Material
{
	layout(offset = 64) vec3 AlbedoColor;
	layout(offset = 80) float Roughness;
} uMaterial;

void main()
{
	outColor = vec4(uMaterial.AlbedoColor, 1.0);
}
