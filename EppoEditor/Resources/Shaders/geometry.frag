#include "Includes/lighting.hlsli"
#include "Includes/platform.hlsli"

struct Camera
{
	float4x4 View;
	float4x4 Projection;
	float4x4 ViewProjection;
	float4 Position;
	float4x4 InverseViewProjection;
};
ConstantBuffer<Camera> uCamera : register(b1, space0);

struct Light
{
	float4 Position; // xyz = world position
	float4 Color;    // rgb = color, a = intensity
};

struct LightData
{
	Light Lights[32];
	uint NumLights;
};
ConstantBuffer<LightData> uLights : register(b2, space0);

struct Environment
{
	float4 ZenithColor;
	float4 HorizonColor;
	float4 GroundColor;
	float4 Params; // x = ambient intensity, y = has skybox
};
ConstantBuffer<Environment> uEnvironment : register(b3, space0);

Texture2D uTextures[] : register(t0, space1);
SamplerState uSampler : register(s0, space0);

struct Input
{
	float3 WorldPos : POSITION0;
	float3 Normal : NORMAL0;
	float2 TexCoord : TEXCOORD0;
};

struct PushConstants
{
	float4x4 Transform;
	uint InstanceOffset;
	int DiffuseMapIndex;
	int NormalMapIndex;
	int RoughMetMapIndex;
	float Metallic;
	float Roughness;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space1);

float4 Main(Input input) : SV_Target
{
	float3 N = normalize(input.Normal);
	float3 V = normalize(uCamera.Position.xyz - input.WorldPos);

	float3 albedo;
	if (uPC.DiffuseMapIndex > -1)
		albedo = uTextures[NonUniformResourceIndex(uPC.DiffuseMapIndex)].Sample(uSampler, input.TexCoord).rgb;
	else
		albedo = float3(1.0, 1.0, 1.0);

	// Direct lighting from each point light, with inverse-square falloff.
	float3 Lo = float3(0.0, 0.0, 0.0);
	for (uint i = 0; i < uLights.NumLights; i++)
	{
		Light l = uLights.Lights[i];
		float3 toLight = l.Position.xyz - input.WorldPos;
		float distanceSq = max(dot(toLight, toLight), 0.0001);
		float3 L = toLight / sqrt(distanceSq);
		float attenuation = 1.0 / distanceSq;
		float3 radiance = l.Color.rgb * l.Color.a * attenuation;
		Lo += BRDF(albedo, L, V, N, uPC.Metallic, uPC.Roughness, radiance);
	}

	// Hemisphere ambient from the sky gradient: ground color below, zenith above.
	// Stands in for skybox-based ambient until an HDR environment is sampled.
	float3 ambient = lerp(uEnvironment.GroundColor.rgb, uEnvironment.ZenithColor.rgb, N.y * 0.5 + 0.5);
	float3 outColor = albedo * ambient * uEnvironment.Params.x;
	outColor += Lo;

	// Gamma correction
	outColor = pow(outColor, float3(0.4545, 0.4545, 0.4545));

	return float4(outColor, 1.0);
}
