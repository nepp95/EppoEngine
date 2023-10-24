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
