#include "Includes/platform.hlsli"
#include "Includes/ibl_cube.hlsli"

struct PushConstants
{
    uint SourceIndex;
    uint SamplerIndex;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

struct Varyings
{
    float4 Position : SV_Position;
    float3 LocalDir : TEXCOORD0;
};

CubeFaceVaryings VSMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    return BuildCubeFaceVertex(vertexID, instanceID);
}

float4 PSMain(Varyings input) : SV_Target
{
    TextureCube source = ResourceDescriptorHeap[NonUniformResourceIndex(uPC.SourceIndex)];
    SamplerState sourceSampler = SamplerDescriptorHeap[NonUniformResourceIndex(uPC.SamplerIndex)];
    return float4(source.SampleLevel(sourceSampler, normalize(input.LocalDir), 0.0).rgb, 1.0);
}
