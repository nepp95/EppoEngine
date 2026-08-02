#include "Includes/fullscreen.hlsli"
#include "Includes/platform.hlsli"

struct PushConstants
{
    float4 Params; // intensity, unused, unused, unused
    uint4 Indices; // scene image, bloom image, sampler, unused
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

FullscreenVaryings VSMain(uint vertexID : SV_VertexID)
{
    return BuildFullscreenTriangleVertex(vertexID);
}

float4 PSMain(FullscreenVaryings input) : SV_Target
{
    Texture2D sceneTexture = ResourceDescriptorHeap[NonUniformResourceIndex(uPC.Indices.x)];
    Texture2D bloomTexture = ResourceDescriptorHeap[NonUniformResourceIndex(uPC.Indices.y)];
    SamplerState sampler = SamplerDescriptorHeap[NonUniformResourceIndex(uPC.Indices.z)];

    const float3 scene = sceneTexture.Sample(sampler, input.TexCoord).rgb;
    const float3 bloom = bloomTexture.Sample(sampler, input.TexCoord).rgb;

    return float4(scene + bloom * max(uPC.Params.x, 0.0), 1.0);
}
