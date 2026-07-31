#include "Includes/lighting.hlsli"
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

float3 CubeFaceDirection(uint face, float2 uv)
{
    if (face == 0u)
        return normalize(float3(1.0, -uv.y, -uv.x));
    if (face == 1u)
        return normalize(float3(-1.0, -uv.y, uv.x));
    if (face == 2u)
        return normalize(float3(uv.x, 1.0, uv.y));
    if (face == 3u)
        return normalize(float3(uv.x, -1.0, -uv.y));
    if (face == 4u)
        return normalize(float3(uv.x, -uv.y, 1.0));
    return normalize(float3(-uv.x, -uv.y, -1.0));
}

float4 Main(Input input) : SV_Target
{
    const float3 N = normalize(input.LocalDir);
    const uint sourceSize = 32u;
    const float sourceMipLevel = log2(uPC.EnvMapSize / float(sourceSize));
    float3 irradiance = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;

    [loop]
    for (uint face = 0u; face < 6u; face++)
    {
        [loop]
        for (uint y = 0u; y < sourceSize; y++)
        {
            [loop]
            for (uint x = 0u; x < sourceSize; x++)
            {
                const float2 uv = (float2(x, y) + 0.5) * (2.0 / float(sourceSize)) - 1.0;
                const float3 sampleDir = CubeFaceDirection(face, uv);
                const float solidAngleWeight = rcp(pow(1.0 + dot(uv, uv), 1.5));
                const float weight = max(dot(N, sampleDir), 0.0) * solidAngleWeight;
                irradiance += uEnvMap.SampleLevel(uSampler, sampleDir, sourceMipLevel).rgb * weight;
                totalWeight += weight;
            }
        }
    }

    return float4(irradiance / max(totalWeight, 0.000001), 1.0);
}
