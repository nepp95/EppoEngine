#include "Includes/platform.hlsli"

struct Input
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

struct PushConstants
{
    float4 Params; // inverse source size xy, threshold, knee
    uint4 Indices; // source image, sampler, apply threshold, unused
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

float4 Main(Input input) : SV_Target
{
    Texture2D source = ResourceDescriptorHeap[NonUniformResourceIndex(uPC.Indices.x)];
    SamplerState sourceSampler = SamplerDescriptorHeap[NonUniformResourceIndex(uPC.Indices.y)];

    const float2 texelSize = uPC.Params.xy;
    const float2 uv = input.TexCoord;

    // 13-tap filter
    float3 a = source.SampleLevel(sourceSampler, uv + float2(-2, -2) * texelSize, 0.0).rgb;
    float3 b = source.SampleLevel(sourceSampler, uv + float2(0, -2) * texelSize, 0.0).rgb;
    float3 c = source.SampleLevel(sourceSampler, uv + float2(2, -2) * texelSize, 0.0).rgb;
    float3 d = source.SampleLevel(sourceSampler, uv + float2(-2, 0) * texelSize, 0.0).rgb;
    float3 e = source.SampleLevel(sourceSampler, uv + float2(0, 0) * texelSize, 0.0).rgb;
    float3 f = source.SampleLevel(sourceSampler, uv + float2(2, 0) * texelSize, 0.0).rgb;
    float3 g = source.SampleLevel(sourceSampler, uv + float2(-2, 2) * texelSize, 0.0).rgb;
    float3 h = source.SampleLevel(sourceSampler, uv + float2(0, 2) * texelSize, 0.0).rgb;
    float3 i = source.SampleLevel(sourceSampler, uv + float2(2, 2) * texelSize, 0.0).rgb;
    float3 j = source.SampleLevel(sourceSampler, uv + float2(-1, -1) * texelSize, 0.0).rgb;
    float3 k = source.SampleLevel(sourceSampler, uv + float2(1, -1) * texelSize, 0.0).rgb;
    float3 l = source.SampleLevel(sourceSampler, uv + float2(-1, 1) * texelSize, 0.0).rgb;
    float3 m = source.SampleLevel(sourceSampler, uv + float2(1, 1) * texelSize, 0.0).rgb;

    float3 result = e * 0.125;
    result += (a + c + g + i) * 0.03125;
    result += (b + d + f + h) * 0.0625;
    result += (j + k + l + m) * 0.125;

    // Soft knee threshold
    if (uPC.Indices.z != 0)
    {
        const float brightness = max(result.r, max(result.g, result.b));
        const float threshold = uPC.Params.z;
        const float safeKnee = max(uPC.Params.w, 0.00001);

        float soft = brightness - threshold + safeKnee;
        soft = clamp(soft, 0.0, 2.0 * safeKnee);
        soft = soft * soft / (4.0 * safeKnee);

        const float contribution = max(brightness - threshold, soft) / max(brightness, 0.00001);

        result *= contribution;
    }

    return float4(result, 1.0);
}
