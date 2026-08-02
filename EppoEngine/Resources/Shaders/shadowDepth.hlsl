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

static const uint s_CascadeCount = 4;
struct Cascade
{
    float4x4 LightViewProjection;
    float SplitDistance;
};
struct ShadowDepthData
{
    Cascade Cascades[s_CascadeCount];
    uint ShadowMapIndex;
    uint ShadowSamplerIndex;
    float DepthBias;
    float NormalBias;
    float InvMapSize;
    float ShadowDistance;
};
ConstantBuffer<ShadowDepthData> uShadowDepth : register(b1, space0);

StructuredBuffer<float4x4> uInstanceTransforms : register(t0, space0);

struct Varyings
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
    uint Layer : SV_RenderTargetArrayIndex;
};

Varyings VSMain(Input input)
{
    Varyings output;

    const uint cascade = input.InstanceID % s_CascadeCount;
    const uint objectInstance = input.InstanceID / s_CascadeCount;

    const float4x4 instanceTransform = uInstanceTransforms[uPC.InstanceOffset + objectInstance];
    const float4x4 worldTransform = mul(instanceTransform, uPC.Transform);
    const float4 worldPosition = mul(worldTransform, float4(input.Position, 1.0));

    output.Position = mul(uShadowDepth.Cascades[cascade].LightViewProjection, worldPosition);
    output.Normal = input.Normal;
    output.TexCoord = input.TexCoord;
    output.Tangent = input.Tangent;
    output.Layer = cascade;

    return output;
}

void PSMain()
{
    // We have no color output with shadow depth
}
