#stage vert
#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outFragPosition;
layout(location = 2) out vec3 outViewPosition;

layout(binding = 0) uniform Camera
{
	mat4 View;
	mat4 Projection;
	mat4 ViewProjection;
} uCamera;

layout(binding = 1) uniform Transform
{
	mat4 Transform;
} uTransform;

void main()
{
	outNormal = inNormal;
	outFragPosition = vec3(uTransform.Transform * vec4(inPosition, 1.0));

	vec4 worldPosition = uTransform.Transform * vec4(inPosition, 1.0);
	outViewPosition = vec3(uCamera.View * vec4(worldPosition.xyz, 1.0));

	gl_Position = uCamera.ViewProjection * uTransform.Transform * vec4(inPosition, 1.0);
}

#stage frag
#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inFragPosition;
layout(location = 2) in vec3 inViewPosition;
layout(location = 0) out vec4 outColor;

layout(binding = 2) uniform Environment
{
	mat4 LightView;
	mat4 LightProjection;
	mat4 LightViewProjection;
	vec3 LightPosition;
	vec3 LightColor;
} uEnvironment;

layout(binding = 3) uniform sampler2D uShadowMap;

layout(binding = 4) uniform Material
{
	vec3 AlbedoColor;
	float Roughness;
} uMaterial;

float CalculateShadow(vec4 fragPos)
{
	// Transform fragPos from clip space to ndc -1 to 1
	vec3 ndcCoords = fragPos.xyz / fragPos.w;

	// Normalize to 0 to 1
	ndcCoords = ndcCoords * 0.5 + 0.5;

	// Get closest depth value 
	float closestDepth = texture(uShadowMap, ndcCoords.xy).r;

	// Get depth of current fragment from light's perspective
	float currentDepth = ndcCoords.z;

	// Check if fragment is in shadow
	float shadow = currentDepth > closestDepth ? 1.0 : 0.0;

	return shadow;
}

void main()
{
	// using the Phong Reflection Model: https://en.wikipedia.org/wiki/Phong_reflection_model

	// Ambient
	// Ia = Ka * I
	// Ia = ambient intensity
	// Ka = ambient intensity coefficient
	// I = light intensity
	float ambientIntensity = 0.1;
	vec3 ambient = ambientIntensity * uEnvironment.LightColor;

	// Diffuse
	// Id = Kd * I * max(dot(N, L), 0)
	// Id = diffuse intensity
	// Kd = diffuse intensity coefficient
	// I = light intensity
	// N = normalized surface normal vector
	// L = normalized light direction vector
	vec3 nNormal = normalize(inNormal);
	vec3 nLightDirection = normalize(uEnvironment.LightPosition - inFragPosition);

	vec3 diffuseIntensity = uEnvironment.LightColor * max(dot(nNormal, nLightDirection), 0.0); // * roughness
	vec3 diffuse = diffuseIntensity * uEnvironment.LightColor;

	// Specular
	// Is = Ks * I * (max(dot(R, V), 0)n
	// Is = specular intensity
	// Ks = specular intensity coefficient
	// I = light intensity
	// R = normalized reflected light direction vector
	// V = normalized view direction vector
	// n = shininess coefficient
	vec3 nViewDirection = normalize(inViewPosition - inFragPosition);
	vec3 nReflectDirection = normalize(reflect(-nLightDirection, nNormal));
	
	float specularIntensity = 0.5;
	vec3 specular = specularIntensity * uEnvironment.LightColor * max(dot(nViewDirection, nReflectDirection), 0.0);

	// Shadow
	vec4 fragPosLightSpace = uEnvironment.LightViewProjection * vec4(inFragPosition, 1.0);
	float shadow = CalculateShadow(fragPosLightSpace);

	// Output
	vec3 result = (ambient + (1.0 - shadow) * (diffuse + specular)) * uMaterial.AlbedoColor;
	outColor = vec4(result, 1.0);
	outColor = vec4(uMaterial.AlbedoColor, 1.0);
}
