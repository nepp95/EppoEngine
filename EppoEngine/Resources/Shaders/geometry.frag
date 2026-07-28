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

struct Input
{
	float3 WorldPos : POSITION0;
	float3 Normal : NORMAL0;
	float2 TexCoord : TEXCOORD0;
	float4 WorldTangent : TANGENT0;
};

struct PushConstants
{
	float4x4 Transform;
	float4 BaseColor;
	uint InstanceOffset;
	int DiffuseMapIndex;
	int NormalMapIndex;
	int RoughMetMapIndex;
	float Metallic;
	float Roughness;
	uint SamplerIndex;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

float4 Main(Input input) : SV_Target
{
	SamplerState samp = SamplerDescriptorHeap[uPC.SamplerIndex];

	// Albedo
	float4 baseColor = uPC.BaseColor;
	if (uPC.DiffuseMapIndex > -1)
	{
	    Texture2D diffuseMap = ResourceDescriptorHeap[NonUniformResourceIndex(uPC.DiffuseMapIndex)];
	    baseColor *= diffuseMap.Sample(samp, input.TexCoord);
	}

	float3 albedo = baseColor.rgb;

	// Metallic roughness
	float metallic = uPC.Metallic;
	float roughness = uPC.Roughness;
	if (uPC.RoughMetMapIndex > -1)
	{
	    Texture2D roughMetMap = ResourceDescriptorHeap[NonUniformResourceIndex(uPC.RoughMetMapIndex)];
	    const float3 rm = roughMetMap.Sample(samp, input.TexCoord).rgb;
	    roughness *= rm.g;
	    metallic *= rm.b;
	}

	// Normal map
	float3 N = normalize(input.Normal);
	if (uPC.NormalMapIndex > -1)
	{
	    Texture2D normalMap = ResourceDescriptorHeap[NonUniformResourceIndex(uPC.NormalMapIndex)];
	    const float3 tangentNormal = normalMap.Sample(samp, input.TexCoord).rgb * 2.0 - 1.0;

	    float3 T = normalize(input.WorldTangent).xyz;
	    T = normalize(T - N * dot(N, T));
	    const float3 B = cross(N, T) * input.WorldTangent.w;
	    N = normalize(mul(tangentNormal, float3x3(T, B, N)));
	}

	const float3 V = normalize(uCamera.Position.xyz - input.WorldPos);

	// Direct lighting
	float3 Lo = float3(0.0, 0.0, 0.0);
	for (uint i = 0; i < uLights.NumLights; i++)
	{
	    Light l = uLights.Lights[i];
	    float3 toLight = l.Position.xyz - input.WorldPos;
	    float distanceSq = max(dot(toLight, toLight), 0.0001);
	    float3 L = toLight / sqrt(distanceSq);
	    float attenuation = 1.0 / distanceSq;
	    float3 radiance = l.Color.rgb * l.Color.a * attenuation;
	    Lo += BRDF(albedo, L, V, N, metallic, roughness, radiance);
	}

	// Hemisphere ambient
	float3 ambient = lerp(uEnvironment.GroundColor.rgb, uEnvironment.ZenithColor.rgb, N.y * 0.5 + 0.5);
	float3 outColor = albedo * ambient * uEnvironment.Params.x;
	outColor += Lo;

	return float4(outColor, 1.0);
}
