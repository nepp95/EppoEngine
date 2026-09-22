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
ConstantBuffer<Camera> uCamera : register(b2, space0);

StructuredBuffer<float4x4> uInstanceTransforms : register(t0, space0);

struct DrawData
{
    float4x4 Transform;
    uint InstanceOffset;
    uint MaterialIndex;
    uint2 Padding;
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

struct Output
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

Output VSMain(Input input)
{
    Output output;
    DrawData draw = uDrawData[uPC.DrawIndex];

    const float4x4 instanceTransform = uInstanceTransforms[draw.InstanceOffset + input.InstanceID];
    const float4x4 worldTransform = mul(instanceTransform, draw.Transform);
    const float4 worldPos = mul(worldTransform, float4(input.Position, 1.0));

    output.Position = mul(uCamera.Projection, mul(uCamera.View, worldPos));
    output.TexCoord = input.TexCoord;

    return output;
}

float4 PSMain(Output input) : SV_Target
{
    DrawData draw = uDrawData[uPC.DrawIndex];
    MaterialData material = uMaterialData[draw.MaterialIndex];

    float4 baseColor = material.BaseColor;
    if (material.DiffuseMapIndex > -1)
    {
        Texture2D diffuseMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.DiffuseMapIndex)];
        SamplerState diffuseSampler = SamplerDescriptorHeap[NonUniformResourceIndex(material.DiffuseSamplerIndex)];
        baseColor *= diffuseMap.Sample(diffuseSampler, input.TexCoord);
    }

    const uint alphaMode = material.Flags & 0x3u;
    if (alphaMode == 1u)
        clip(baseColor.a - material.AlphaCutoff);

    return baseColor;
}
