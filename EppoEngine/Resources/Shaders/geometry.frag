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
	uint4 IBL0;    // x = env cube, y = irradiance, z = prefilter, w = BRDF LUT bindless indices
	uint4 IBL1;    // x = IBL sampler index
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

	// Ambient: image-based lighting when a skybox is baked, gradient otherwise.
	const float dotNV = max(dot(N, V), 0.0);
	float3 ambient;
	if (uEnvironment.Params.y > 0.5)
	{
		TextureCube irradianceMap = ResourceDescriptorHeap[uEnvironment.IBL0.y];
		TextureCube prefilterMap = ResourceDescriptorHeap[uEnvironment.IBL0.z];
		Texture2D brdfLut = ResourceDescriptorHeap[uEnvironment.IBL0.w];
		SamplerState iblSamp = SamplerDescriptorHeap[uEnvironment.IBL1.x];

		// Fresnel-Schlick with roughness (constant F90 is wrong at glancing angles on rough surfaces).
		const float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
		const float3 Fmax = max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0);
		const float3 F = F0 + (Fmax - F0) * pow(1.0 - dotNV, 5.0);
		const float3 kD = (1.0 - F) * (1.0 - metallic);

		const float3 irradiance = irradianceMap.SampleLevel(iblSamp, N, 0).rgb;
		const float3 diffuse = irradiance * albedo;

		const float3 R = reflect(-V, N);
		const float maxLod = 4.0; // prefilter mip count - 1
		const float3 prefiltered = prefilterMap.SampleLevel(iblSamp, R, roughness * maxLod).rgb;
		const float2 brdf = brdfLut.SampleLevel(iblSamp, float2(dotNV, roughness), 0).rg;
		const float3 specular = prefiltered * (F * brdf.x + brdf.y);

		ambient = (kD * diffuse + specular) * uEnvironment.Params.x;
	}
	else
	{
		// Gradient fallback.
		ambient = albedo * lerp(uEnvironment.GroundColor.rgb, uEnvironment.ZenithColor.rgb, N.y * 0.5 + 0.5) * uEnvironment.Params.x;
	}

	float3 outColor = ambient + Lo;
	return float4(outColor, 1.0);
}
