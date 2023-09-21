#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Material
{
	vec3 AlbedoColor;
} uMaterial;

void main()
{
	outColor = vec4(uMaterial.AlbedoColor, 1.0);
}
