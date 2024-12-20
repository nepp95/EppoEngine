#stage vert
#version 450

#include "Includes/base.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outFragPos;

layout(push_constant) uniform Transform
{
	layout(offset = 0) mat4 Transform;
	layout(offset = 64) vec4 DiffuseColor;
	layout(offset = 80) int DiffuseMapIndex;
    layout(offset = 84) int NormalMapIndex;
    layout(offset = 88) int RoughnessMetallicMapIndex;
} uTransform;

void main()
{
	outNormal = inNormal;
    outTexCoord = inTexCoord;
    outFragPos = vec3(uTransform.Transform * vec4(inPosition, 1.0));

	gl_Position = uCamera.ViewProjection * uTransform.Transform * vec4(inPosition, 1.0);
}

#stage frag
#version 450

#include "Includes/base.glsl"
#include "Includes/constants.glsl"
#include "Includes/lighting.glsl"

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inFragPos;

layout(location = 0) out vec4 outFragColor;
layout(location = 1) out vec4 outDepthColor;
layout(location = 2) out vec4 outNormalColor;

layout(push_constant) uniform Material
{
	layout(offset = 0) mat4 Transform;
	layout(offset = 64) vec4 DiffuseColor;
	layout(offset = 80) int DiffuseMapIndex;
    layout(offset = 84) int NormalMapIndex;
    layout(offset = 88) int RoughnessMetallicMapIndex;
} uMaterial;

void main()
{
	vec3 diffuse = vec3(0.0);
	if (uMaterial.DiffuseMapIndex > 0)
		diffuse = texture(uMaterialTex[uMaterial.DiffuseMapIndex], inTexCoord).rgb;
	else
		diffuse = uMaterial.DiffuseColor.rgb;

    vec3 metallicTexColor = texture(uMaterialTex[uMaterial.RoughnessMetallicMapIndex], inTexCoord).rgb;

    float metallic = metallicTexColor.b;
    float roughness = metallicTexColor.g;
    float ao = 1.0;

    // Theory by: https://learnopengl.com/PBR/Lighting
    vec3 N = normalize(inNormal);
    vec3 V = normalize(uCamera.Position.xyz - inFragPos);

    // Calculate reflectance at normal incidence
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, diffuse, metallic);

    // Reflectance equation
    vec3 Lo = vec3(0.0);
	float accumulatedDepth = 0.0;
	for (int i = 0; i < uLights.NumLights; ++i)
    {
        // Calculate per light radiance
        vec3 L = normalize(uLights.Lights[i].Position.xyz - inFragPos);
        vec3 H = normalize(V + L);

        float distance = length(uLights.Lights[i].Position.xyz - inFragPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = uLights.Lights[i].Color.rgb * attenuation * 10.0;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);

		accumulatedDepth += CalculateShadowDepth(inFragPos, 50.0, i);
		float shadow = CalculateShadow(inFragPos, 50.0, i);
        Lo += shadow * ((kD * diffuse / PI + specular) * radiance * NdotL);
    }

    vec3 ambient = vec3(0.03) * diffuse * ao;
    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));

    // Gamma correction
    //color = pow(color, vec3(1.0 / 2.2));

	outFragColor = vec4(color, 1.0);
	outDepthColor = vec4(vec3(accumulatedDepth), 1.0);
	outNormalColor = vec4(N, 1.0);
}
