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
	vec4 Position;
} uCamera;

layout(binding = 1) uniform Transform
{
	mat4 Transform;
} uTransform;

layout(binding = 2) uniform DirectionalLight
{
	mat4 View;
	mat4 Projection;
	vec4 Direction;
	vec4 AmbientColor;
	vec4 DiffuseColor;
	vec4 SpecularColor;
} uDirectionalLight;

void main()
{
	outNormal = mat3(transpose(inverse(uTransform.Transform))) * inNormal;
	outTexCoord = inTexCoord;
	outFragPosition = vec3(uTransform.Transform * vec4(inPosition, 1.0));
	outFragPosLightSpace = uDirectionalLight.Projection * uDirectionalLight.View * vec4(outFragPosition, 1.0);

	gl_Position = uCamera.ViewProjection * vec4(outFragPosition, 1.0);
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
	vec4 Position;
} uCamera;

layout(binding = 2) uniform DirectionalLight
{
	mat4 View;
	mat4 Projection;
	vec4 Direction;
	vec4 AmbientColor;
	vec4 DiffuseColor;
	vec4 SpecularColor;
} uDirectionalLight;

layout(binding = 4) uniform Material
{
	vec4 AmbientColor;
	vec4 DiffuseColor;
	vec4 SpecularColor;
	float Roughness;
} uMaterial;

float CalculateShadow(vec4 fragPos, vec3 normal, vec3 lightDir);

void main()
{
	// using the Phong Reflection Model: https://en.wikipedia.org/wiki/Phong_reflection_model

	// Ambient
	vec3 ambient = uDirectionalLight.AmbientColor.rgb;

	// Diffuse
	vec3 nNormal = normalize(inNormal);
	vec3 nLightDirection = normalize(-uDirectionalLight.Direction.xyz);

	float diffuseIntensity = max(dot(nNormal, nLightDirection), 0.0);
	vec3 diffuse = diffuseIntensity * uDirectionalLight.DiffuseColor.rgb;

	// Specular
	vec3 nViewDirection = normalize(uCamera.Position.xyz - inFragPosition);
	vec3 reflectDirection = reflect(-nLightDirection, nNormal);

	float spec = pow(max(dot(nViewDirection, reflectDirection), 0.0), uMaterial.Roughness);
	vec3 specular = spec * uDirectionalLight.SpecularColor.rgb;

	// Shadow
	float shadow = CalculateShadow(inFragPosLightSpace, nNormal, nLightDirection);

	// Output
	vec3 result = (ambient + (1.0 - shadow) * (diffuse + specular)) * uMaterial.DiffuseColor.rgb;
	outColor = vec4(result, 1.0);
}

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
	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);

	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(uShadowMap, ndcCoords.xy + vec2(x, y) * texelSize).r;
			shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
		}
	}

	shadow /= 9.0;

	if (ndcCoords.z > 1.0)
		shadow = 0.0;

	return shadow;
}
