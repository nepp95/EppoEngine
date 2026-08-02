#include "Includes/fullscreen.hlsli"
#include "Includes/platform.hlsli"

struct PushConstants
{
    float4 Params; // inverse source size xy, radius, unused
    uint4 Indices; // source image, sampler, unused, unused
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

FullscreenVaryings VSMain(uint vertexID : SV_VertexID)
{
    return BuildFullscreenTriangleVertex(vertexID);
}

float4 PSMain(FullscreenVaryings input) : SV_Target
{
    Texture2D source = ResourceDescriptorHeap[NonUniformResourceIndex(uPC.Indices.x)];
    SamplerState sourceSampler = SamplerDescriptorHeap[NonUniformResourceIndex(uPC.Indices.y)];

    const float2 scale = uPC.Params.xy * max(uPC.Params.z, 0.0);
    const float2 uv = input.TexCoord;

    // 3x3 tent filter
    float3 a = source.SampleLevel(sourceSampler, uv + float2(-1, -1) * scale, 0.0).rgb;
    float3 b = source.SampleLevel(sourceSampler, uv + float2(0, -1) * scale, 0.0).rgb;
    float3 c = source.SampleLevel(sourceSampler, uv + float2(1, -1) * scale, 0.0).rgb;
    float3 d = source.SampleLevel(sourceSampler, uv + float2(-1, 0) * scale, 0.0).rgb;
    float3 e = source.SampleLevel(sourceSampler, uv + float2(0, 0) * scale, 0.0).rgb;
    float3 f = source.SampleLevel(sourceSampler, uv + float2(1, 0) * scale, 0.0).rgb;
    float3 g = source.SampleLevel(sourceSampler, uv + float2(-1, 1) * scale, 0.0).rgb;
    float3 h = source.SampleLevel(sourceSampler, uv + float2(0, 1) * scale, 0.0).rgb;
    float3 i = source.SampleLevel(sourceSampler, uv + float2(1, 1) * scale, 0.0).rgb;

    float3 result = e * 4.0;
    result += (b + d + f + h) * 2.0;
    result += (a + c + g + i) * 1.0;
    result *= 1.0 / 16.0;

    return float4(result, 1.0);
}
