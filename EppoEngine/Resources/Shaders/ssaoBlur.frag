#include "Includes/platform.hlsli"

struct Input
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

struct PushConstants
{
    float2 Direction;
    float2 InvSize;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

struct Camera
{
	float4x4 View;
	float4x4 Projection;
	float4x4 ViewProjection;
	float4x4 InverseViewProjection;
	float4 Position;
};
ConstantBuffer<Camera> uCamera : register(b1, space0);

Texture2D uAO : register(t0, space0);
Texture2D uDepth : register(t1, space0);
SamplerState uSampler : register(s0, space0);

static const float s_SpatialWeights[5] = {
    0.2270270270,
    0.1945945946,
    0.1216216216,
    0.0540540541,
    0.0162162162
};

float ReconstructViewDepth(float2 uv, float depth)
{
    const float2 ndcXY = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    float4 world = mul(uCamera.InverseViewProjection, float4(ndcXY, depth, 1.0));
    world /= world.w;
    return -mul(uCamera.View, world).z;
}

float Main(Input input) : SV_Target
{
	const float centerDepth = uDepth.SampleLevel(uSampler, input.TexCoord, 0);
	if (centerDepth >= 1.0)
	    return 1.0;

    const float centerViewDepth = ReconstructViewDepth(input.TexCoord, centerDepth);
    float weightedAo = 0.0;
    float totalWeight = 0.0;

    [unroll]
    for (int offset = -4; offset <= 4; offset++)
    {
        const float2 tapUv = input.TexCoord + float(offset) * uPC.Direction * uPC.InvSize;
        const float tapDepth = uDepth.SampleLevel(uSampler, tapUv, 0);
        if (tapDepth >= 1.0)
            continue;

        const float tapViewDepth = ReconstructViewDepth(tapUv, tapDepth);
        const float spatialWeight = s_SpatialWeights[abs(offset)];
        const float depthWeight = exp(-abs(tapViewDepth - centerViewDepth) * 20.0);
        const float weight = spatialWeight * depthWeight;

        weightedAo += uAO.SampleLevel(uSampler, tapUv, 0) * weight;
        totalWeight += weight;
    }

    return weightedAo / max(totalWeight, 1e-5);
}
