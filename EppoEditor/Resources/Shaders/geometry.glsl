#stage vert
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outFragPosition;
layout(location = 2) out vec3 outViewPosition;

layout(set = 1, binding = 0) uniform Camera
{
	mat4 ViewMatrix;
	mat4 ViewProjection;
} uCamera;

layout(push_constant) uniform Transform
{
	layout(offset = 0) mat4 Transform;
} uTransform;

void main()
{
	outNormal = inNormal;
	outFragPosition = vec3(uTransform.Transform * vec4(inPosition, 1.0));

	vec4 worldPosition = uTransform.Transform * vec4(inPosition, 1.0);
	outViewPosition = vec3(uCamera.ViewMatrix * vec4(worldPosition.xyz, 1.0));

	gl_Position = uCamera.ViewProjection * uTransform.Transform * vec4(inPosition, 1.0);
}

#stage frag
#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inFragPosition;
layout(location = 2) in vec3 inViewPosition;
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

	// Specular
	float specularStrength = 0.5;
	vec3 viewDirection = normalize(inViewPosition - inFragPosition);
	vec3 reflectDirection = reflect(-lightDirection, norm);

	float spec = pow(max(dot(viewDirection, reflectDirection), 0.0), 32);
	vec3 specular = specularStrength * spec * lightColor;

	vec3 result = (ambient + diffuse + specular) * uMaterial.AlbedoColor;

	outColor = vec4(result, 1.0);
}
