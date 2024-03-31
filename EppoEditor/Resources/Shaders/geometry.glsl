#stage vert
#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outFragPosition;
layout(location = 3) out vec4 outFragPosLightSpace;

layout(binding = 0) uniform Camera
{
	mat4 View;
	mat4 Projection;
	mat4 ViewProjection;
	vec3 Position;
} uCamera;

layout(binding = 1) uniform Transform
{
	mat4 Transform;
} uTransform;

layout(binding = 2) uniform Environment
{
	mat4 LightView;
	mat4 LightProjection;
	mat4 LightViewProjection;
	vec3 LightPosition;
	vec3 LightColor;
} uEnvironment;

void main()
{
	outNormal = transpose(inverse(mat3(uTransform.Transform))) * inNormal;
	outTexCoord = inTexCoord;
	outFragPosition = vec3(uTransform.Transform * vec4(inPosition, 1.0));
	outFragPosLightSpace = uEnvironment.LightViewProjection * vec4(outFragPosition, 1.0);

	gl_Position = uCamera.ViewProjection * uTransform.Transform * vec4(inPosition, 1.0); // outFragPosition <<?
}

#stage frag
#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inFragPosition;
layout(location = 3) in vec4 inFragPosLightSpace;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uShadowMap;

layout(binding = 0) uniform Camera
{
	mat4 View;
	mat4 Projection;
	mat4 ViewProjection;
	vec3 Position;
} uCamera;

layout(binding = 2) uniform Environment
{
	mat4 LightView;
	mat4 LightProjection;
	mat4 LightViewProjection;
	vec3 LightPosition;
	vec3 LightColor;
} uEnvironment;

layout(binding = 4) uniform Material
{
	vec3 AlbedoColor;
	float Roughness;
} uMaterial;

float CalculateShadow(vec4 fragPos, vec3 normal, vec3 lightDir)
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
	float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

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
	float ambientIntensity = 0.2;
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

	float diffuseIntensity = max(dot(nLightDirection, nNormal), 0.0);
	vec3 diffuse = diffuseIntensity * uEnvironment.LightColor;

	// Specular
	// Is = Ks * I * (max(dot(R, V), 0)n
	// Is = specular intensity
	// Ks = specular intensity coefficient
	// I = light intensity
	// R = normalized reflected light direction vector
	// V = normalized view direction vector
	// n = shininess coefficient
	vec3 nViewDirection = normalize(uCamera.Position - inFragPosition);
	vec3 nHalfwayDirection = normalize(nLightDirection + nViewDirection);

	float spec = pow(max(dot(nNormal, nHalfwayDirection), 0.0), 64.0);
	vec3 specular = spec * uEnvironment.LightColor;

	// Shadow
	float shadow = CalculateShadow(inFragPosLightSpace, nNormal, nLightDirection);

	// Output
	vec3 result = (ambient + (1.0 - shadow) * (diffuse + specular)) * uMaterial.AlbedoColor;
	outColor = vec4(result, 1.0);
}
