#include "Includes/platform.hlsli"

struct Input
{
    float3 Position : POSITION0;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
    uint InstanceID : SV_InstanceID;
};

struct PushConstants
{
    float4x4 Transform;
    uint InstanceOffset;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

struct ShadowDepthData
{
    float4x4 LightViewProjection;
    uint4 Indices; // shadow map, sampler, enabled, unused
    float4 Params; // bias, inverse map size, unused, unused
};
ConstantBuffer<ShadowDepthData> uShadowDepth : register(b1, space0);

StructuredBuffer<float4x4> uInstanceTransforms : register(t0, space0);

struct Varyings
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
};

Varyings VSMain(Input input)
{
    Varyings output;

    const float4x4 instanceTransform = uInstanceTransforms[uPC.InstanceOffset + input.InstanceID];
    const float4x4 worldTransform = mul(instanceTransform, uPC.Transform);
    const float4 worldPosition = mul(worldTransform, float4(input.Position, 1.0));

    output.Position = mul(uShadowDepth.LightViewProjection, worldPosition);
    output.Normal = input.Normal;
    output.TexCoord = input.TexCoord;
    output.Tangent = input.Tangent;

    return output;
}

void PSMain()
{
    // We have no color output with shadow depth
}
