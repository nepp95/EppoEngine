#include "Includes/lighting.hlsli"
#include "Includes/ibl.hlsli"
#include "Includes/platform.hlsli"

TextureCube uEnvMap : register(t0, space0);
SamplerState uSampler : register(s0, space0);

struct PushConstants
{
	float Roughness;
	float EnvMapSize;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

struct Input
{
	float4 Position : SV_Position;
	float3 LocalDir : TEXCOORD0;
};

float4 Main(Input input) : SV_Target
{
	const float3 N = normalize(input.LocalDir);
	const float3 V = N; // split-sum assumes V = R = N

	const uint sampleCount = 1024u;
	float3 prefiltered = float3(0.0, 0.0, 0.0);
	float totalWeight = 0.0;

	for (uint i = 0u; i < sampleCount; i++)
	{
		const float2 Xi = Hammersley(i, sampleCount);
		const float3 H = ImportanceSampleGGX(Xi, N, uPC.Roughness);
		const float3 L = normalize(2.0 * dot(V, H) * H - V);

		const float dotNL = max(dot(N, L), 0.0);
		if (dotNL > 0.0)
		{
			float mipLevel = 0.0;
			if (uPC.Roughness > 0.0)
			{
				const float dotNH = max(dot(N, H), 0.0);
				const float dotVH = max(dot(V, H), 0.0);
				const float pdf = DGGX(dotNH, uPC.Roughness) * dotNH / max(4.0 * dotVH, 0.000001);
				mipLevel = SourceMipLevel(sampleCount, pdf, uPC.EnvMapSize);
			}
			prefiltered += uEnvMap.SampleLevel(uSampler, L, mipLevel).rgb * dotNL;
			totalWeight += dotNL;
		}
	}

	return float4(prefiltered / max(totalWeight, 0.0001), 1.0);
}
