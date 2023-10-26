#version 450

layout(location = 0) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Material
{
	layout(offset = 64) vec3 AlbedoColor;
	layout(offset = 80) float Roughness;
} uMaterial;

void main()
{
	vec3 lightColor = vec3(1.0, 1.0, 1.0);
	vec3 lightPosition = vec3(-3.0, -2.0, 0.0);
	
	// Ambient
	float ambientStrength = 0.1;
	vec3 ambient = ambientStrength * lightColor;
	
	// Diffuse
	vec3 norm = normalize(inNormal);
	vec3 lightDirection = normalize(lightPosition);

	float diff = max(dot(norm, lightDirection), 0.0);
	vec3 diffuse = diff * lightColor;

	vec3 result = (ambient + diffuse) * uMaterial.AlbedoColor;

	outColor = vec4(result, 1.0);
}
