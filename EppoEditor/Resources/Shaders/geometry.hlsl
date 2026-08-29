#include "Includes/lighting.hlsli"
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
    float3 WorldPos : POSITION0;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 WorldTangent : TANGENT0;
};

Output VSMain(Input input)
{
    Output output;
    DrawData draw = uDrawData[uPC.DrawIndex];

    const float4x4 instanceTransform = uInstanceTransforms[draw.InstanceOffset + input.InstanceID];
    const float4x4 worldTransform = mul(instanceTransform, draw.Transform);

    output.WorldPos = mul(worldTransform, float4(input.Position, 1.0)).xyz;
    output.Normal = mul(InverseTranspose3x3((float3x3)worldTransform), input.Normal);
    output.Position = mul(uCamera.Projection, mul(uCamera.View, float4(output.WorldPos, 1.0)));
    output.TexCoord = input.TexCoord;
    output.WorldTangent = float4(mul((float3x3)worldTransform, input.Tangent.xyz), input.Tangent.w);

    return output;
}

struct GBuffer
{
    float4 BaseColorMetalness : SV_Target0;
    float4 NormalRoughness : SV_Target1;
    float4 EmissionAO : SV_Target2;
};

GBuffer PSMain(Output input, bool isFrontFace : SV_IsFrontFace)
{
    DrawData draw = uDrawData[uPC.DrawIndex];
    MaterialData material = uMaterialData[draw.MaterialIndex];

    // Albedo
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

    float3 albedo = baseColor.rgb;

    // Metallic roughness
    float metallic = material.Metallic;
    float roughness = material.Roughness;
    if (material.RoughMetMapIndex > -1)
    {
        Texture2D roughMetMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.RoughMetMapIndex)];
        SamplerState roughMetSampler = SamplerDescriptorHeap[NonUniformResourceIndex(material.RoughMetSamplerIndex)];
        const float3 rm = roughMetMap.Sample(roughMetSampler, input.TexCoord).rgb;
        roughness *= rm.g;
        metallic *= rm.b;
    }

    // Normal map
    const float3 geometricN = normalize(input.Normal);
    float3 N = geometricN;
    if (material.NormalMapIndex > -1)
    {
        Texture2D normalMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.NormalMapIndex)];
        SamplerState normalSampler = SamplerDescriptorHeap[NonUniformResourceIndex(material.NormalSamplerIndex)];
        float3 tangentNormal = normalMap.Sample(normalSampler, input.TexCoord).rgb * 2.0 - 1.0;
        tangentNormal.xy *= material.NormalScale;
        tangentNormal = normalize(tangentNormal);

        float3 T = normalize(input.WorldTangent).xyz;
        T = normalize(T - N * dot(N, T));
        const float3 B = cross(N, T) * input.WorldTangent.w;
        N = normalize(mul(tangentNormal, float3x3(T, B, N)));
    }
    
    const bool doubleSided = (material.Flags & (1u << 2u)) != 0u;
    if (doubleSided && !isFrontFace)
        N = -N;

    // Ambient occlusion
        float materialAO = 1.0;
    if (material.AOMapIndex > -1)
    {
        Texture2D aoMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.AOMapIndex)];
        SamplerState aoSampler = SamplerDescriptorHeap[NonUniformResourceIndex(material.AOSamplerIndex)];
        materialAO = aoMap.Sample(aoSampler, input.TexCoord).r;
    }

    // Emissive
    float3 emissive = material.EmissiveFactor;
    if (material.EmissiveMapIndex > -1)
    {
        Texture2D emissiveMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.EmissiveMapIndex)];
        SamplerState emissiveSampler = SamplerDescriptorHeap[NonUniformResourceIndex(material.EmissiveSamplerIndex)];
        emissive *= emissiveMap.Sample(emissiveSampler, input.TexCoord).rgb;
    }

    GBuffer output;
    output.BaseColorMetalness = float4(albedo, saturate(metallic));
    output.NormalRoughness = float4(normalize(N) * 0.5 + 0.5, saturate(roughness));
    output.EmissionAO = float4(emissive, saturate(materialAO));
    return output;
}
