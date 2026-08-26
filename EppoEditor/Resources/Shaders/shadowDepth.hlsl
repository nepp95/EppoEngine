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
    uint DrawIndex;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

static const uint s_CascadeCount = 4;
struct Cascade
{
    float4x4 LightViewProjection;
    float SplitDistance;
    float WorldUnitsPerTexel;
    float TransitionStart;
    float Padding;
};
struct ShadowDepthData
{
    Cascade Cascades[s_CascadeCount];
    uint ShadowMapIndex;
    uint ShadowSamplerIndex;
    float DepthBiasTexels;
    float NormalBiasTexels;
    float InvMapSize;
    float ShadowDistance;
};
ConstantBuffer<ShadowDepthData> uShadowDepth : register(b1, space0);

StructuredBuffer<float4x4> uInstanceTransforms : register(t0, space0);

struct DrawData
{
    float4x4 Transform;
    uint InstanceOffset;
    uint MaterialIndex;
};
StructuredBuffer<DrawData> uDrawData : register(t1, space0);

struct MaterialData
{
    int DiffuseMapIndex;
    int NormalMapIndex;
    int RoughMetMapIndex;
    int AOMapIndex;
    int EmissiveMapIndex;
    uint DiffuseSamplerIndex;
    uint NormalSamplerIndex;
    uint RoughMetSamplerIndex;
    uint AOSamplerIndex;
    uint EmissiveSamplerIndex;
    uint2 Padding0;
    float4 BaseColor;
    float3 EmissiveFactor;
    float Metallic;
    float Roughness;
    float NormalScale;
    float AlphaCutoff;
    uint Flags;
};
StructuredBuffer<MaterialData> uMaterialData : register(t2, space0);

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

    const DrawData draw = uDrawData[uPC.DrawIndex];
    
    const uint cascade = input.InstanceID % s_CascadeCount;
    const uint objectInstance = input.InstanceID / s_CascadeCount;

    const float4x4 instanceTransform = uInstanceTransforms[draw.InstanceOffset + objectInstance];
    const float4x4 worldTransform = mul(instanceTransform, draw.Transform);
    const float4 worldPosition = mul(worldTransform, float4(input.Position, 1.0));

    output.Position = mul(uShadowDepth.Cascades[cascade].LightViewProjection, worldPosition);
    output.Normal = input.Normal;
    output.TexCoord = input.TexCoord;
    output.Tangent = input.Tangent;
    output.Layer = cascade;

    return output;
}

void PSMain(Varyings input)
{
    const DrawData draw = uDrawData[uPC.DrawIndex];
    const MaterialData material = uMaterialData[draw.MaterialIndex];
    const uint alphaMode = material.Flags & 0x3u;
    
    if (alphaMode != 1u)
        return;
    
    float alpha = material.BaseColor.a;
    
    if (material.DiffuseMapIndex > -1)
    {
        Texture2D diffuseMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.DiffuseMapIndex)];
        SamplerState diffuseSampler = SamplerDescriptorHeap[NonUniformResourceIndex(material.DiffuseSamplerIndex)];
        alpha *= diffuseMap.Sample(diffuseSampler, input.TexCoord).a;
    }
    
    clip(alpha - material.AlphaCutoff);
}
