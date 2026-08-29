#include "Includes/fullscreen.hlsli"
#include "Includes/platform.hlsli"

struct PushConstants
{
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
    float NearClip;
    float FarClip;
};
ConstantBuffer<Camera> uCamera : register(b1, space0);

Texture2D uAO : register(t0, space0);
Texture2D uDepth : register(t1, space0);
SamplerState uSampler : register(s0, space0);

float ReconstructViewDepth(float2 uv, float depth)
{
    const float2 ndcXY = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    float4 world = mul(uCamera.InverseViewProjection, float4(ndcXY, depth, 1.0));
    world /= world.w;
    return -mul(uCamera.View, world).z;
}

FullscreenVaryings VSMain(uint vertexID : SV_VertexID)
{
    return BuildFullscreenTriangleVertex(vertexID);
}

float PSMain(FullscreenVaryings input) : SV_Target
{
    const float centerDepth = uDepth.SampleLevel(uSampler, input.TexCoord, 0).r;
    if (centerDepth >= 1.0)
        return 1.0;

    const float centerViewDepth = ReconstructViewDepth(input.TexCoord, centerDepth);
    float weightedAo = 0.0;
    float totalWeight = 0.0;
    
    [unroll]
    for (int y = -2; y <= 2; y++)
    {
        [unroll]
        for (int x = -2; x <= 2; x++)
        {
            const float2 tapUv = input.TexCoord + float2(x, y) * uPC.InvSize;
            const float tapDepth = uDepth.SampleLevel(uSampler, tapUv, 0).r;
            if (tapDepth >= 1.0)
                continue;
            
            const float tapViewDepth = ReconstructViewDepth(tapUv, tapDepth);
            const float spatialWeight = exp(-0.5 * float(x * x + y * y));
            const float depthWeight = exp(-abs(tapViewDepth - centerViewDepth) * 20.0f);
            const float weight = spatialWeight * depthWeight;
            weightedAo += uAO.SampleLevel(uSampler, tapUv, 0).r * weight;
            totalWeight += weight;
        }
    }

    return weightedAo / max(totalWeight, 1e-5);
}
