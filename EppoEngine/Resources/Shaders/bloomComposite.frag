#include "Includes/platform.hlsli"

struct Input
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

struct PushConstants
{
    float4 Params; // intensity, unused, unused, unused
    uint4 Indices; // scene image, bloom image, sampler, unused
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

float4 Main(Input input) : SV_Target
{
    Texture2D sceneTexture = ResourceDescriptorHeap[NonUniformResourceIndex(uPC.Indices.x)];
    Texture2D bloomTexture = ResourceDescriptorHeap[NonUniformResourceIndex(uPC.Indices.y)];
    SamplerState sampler = SamplerDescriptorHeap[NonUniformResourceIndex(uPC.Indices.z)];

    const float3 scene = sceneTexture.Sample(sampler, input.TexCoord).rgb;
    const float3 bloom = bloomTexture.Sample(sampler, input.TexCoord).rgb;

    return float4(scene + bloom * max(uPC.Params.x, 0.0), 1.0);
}
