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

struct DirectionalLight
{
    float4 Direction;   // xyz = world-space direction the light travels
    float4 Color;       // rgb = color, a = intensity
};

struct Light
{
    float4 Position;    // xyz = world position
    float4 Color;       // rgb = color, a = intensity
};

struct LightData
{
    DirectionalLight DirLight;
    Light Lights[32];
    uint NumLights;
    uint HasDirLight;
};
ConstantBuffer<LightData> uLights : register(b3, space0);

struct Environment
{
    float4 ZenithColor;
    float4 HorizonColor;
    float4 GroundColor;
    float4 Params; // x = ambient intensity, y = has skybox
    uint4 IBL0;    // x = env cube, y = irradiance, z = prefilter, w = BRDF LUT bindless indices
    uint4 IBL1;    // x = IBL sampler index
};
ConstantBuffer<Environment> uEnvironment : register(b4, space0);

static const uint s_KernelSize = 32;
struct Ssao
{
    float4 Kernel[s_KernelSize];
    float4 Params;
    float4 InvSize;
};
ConstantBuffer<Ssao> uSsao : register(b5, space0);

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
    uint2 Padding1;
};
StructuredBuffer<MaterialData> uMaterialData : register(t2, space0);

Texture2D uSsaoTex : register(t3, space0);
SamplerState uSsaoSampler : register(s1, space0);

float CalcShadowFactor(const float3 worldPosition, const float3 worldNormal)
{
    const float viewDepth = -mul(uCamera.View, float4(worldPosition, 1.0)).z;
    if (viewDepth <= 0.0 || viewDepth > uShadowDepth.ShadowDistance)
        return 1.0;

    uint cascade = 0;
    while (cascade + 1 < s_CascadeCount && viewDepth > uShadowDepth.Cascades[cascade].SplitDistance)
        cascade++;

    const float3 biasedWorldPosition = worldPosition + normalize(worldNormal) * uShadowDepth.NormalBias;
    const float4 lightClip = mul(uShadowDepth.Cascades[cascade].LightViewProjection, float4(biasedWorldPosition, 1.0));
    if (lightClip.w <= 0.0)
        return 1.0;

    const float3 lightNdc = lightClip.xyz / lightClip.w;
    if (any(lightNdc.xy < -1.0) || any(lightNdc.xy > 1.0) || lightNdc.z < 0.0 || lightNdc.z > 1.0)
        return 1.0;

    const float2 uv = lightNdc.xy * float2(0.5, -0.5) + 0.5;

    Texture2DArray<float> shadowMap = ResourceDescriptorHeap[uShadowDepth.ShadowMapIndex];
    SamplerState shadowSampler = SamplerDescriptorHeap[uShadowDepth.ShadowSamplerIndex];
    float visibleSamples = 0.0;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            const float3 sampleUv = float3(uv + float2(x, y) * uShadowDepth.InvMapSize, cascade);
            const float storedDepth = shadowMap.SampleLevel(shadowSampler, sampleUv, 0);
            visibleSamples += lightNdc.z - uShadowDepth.DepthBias <= storedDepth ? 1.0 : 0.0;
        }
    }

    return visibleSamples / 9.0;
}

struct Varyings
{
    float4 Position : SV_Position;
    float3 WorldPos : POSITION0;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 WorldTangent : TANGENT0;
};

Varyings VSMain(Input input)
{
    Varyings output;
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

float4 PSMain(Varyings input) : SV_Target
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

    const float3 V = normalize(uCamera.Position.xyz - input.WorldPos);

    // Directional lighting
    float3 Lo = float3(0.0, 0.0, 0.0);

    if (uLights.HasDirLight)
    {
        const float3 L = normalize(-uLights.DirLight.Direction.xyz);
        const float3 radiance = uLights.DirLight.Color.rgb * uLights.DirLight.Color.a;
        Lo += CalcShadowFactor(input.WorldPos, geometricN) * BRDF(albedo, L, V, N, metallic, roughness, radiance);
    }

    // Point lights
    for (uint i = 0; i < uLights.NumLights; i++)
    {
        Light l = uLights.Lights[i];
        float3 toLight = l.Position.xyz - input.WorldPos;
        float distanceSq = max(dot(toLight, toLight), 0.0001);
        float3 L = toLight / sqrt(distanceSq);
        float attenuation = 1.0 / distanceSq;
        float3 radiance = l.Color.rgb * l.Color.a * attenuation;
        Lo += BRDF(albedo, L, V, N, metallic, roughness, radiance);
    }

    // Ambient: image-based lighting when a skybox is baked, gradient otherwise.
    const float dotNV = max(dot(N, V), 0.0);
    float3 ambient;
    if (uEnvironment.Params.y > 0.5)
    {
        TextureCube irradianceMap = ResourceDescriptorHeap[uEnvironment.IBL0.y];
        TextureCube prefilterMap = ResourceDescriptorHeap[uEnvironment.IBL0.z];
        Texture2D brdfLut = ResourceDescriptorHeap[uEnvironment.IBL0.w];
        SamplerState iblSamp = SamplerDescriptorHeap[uEnvironment.IBL1.x];

        // Fresnel-Schlick with roughness (constant F90 is wrong at glancing angles on rough surfaces).
        const float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
        const float3 Fmax = max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0);
        const float3 F = F0 + (Fmax - F0) * pow(1.0 - dotNV, 5.0);
        const float3 kD = (1.0 - F) * (1.0 - metallic);

        const float3 irradiance = irradianceMap.SampleLevel(iblSamp, N, 0).rgb;
        const float3 diffuse = irradiance * albedo;

        const float3 R = reflect(-V, N);
        const float maxLod = 4.0; // prefilter mip count - 1
        const float3 prefiltered = prefilterMap.SampleLevel(iblSamp, R, roughness * maxLod).rgb;
        const float2 brdf = brdfLut.SampleLevel(iblSamp, float2(dotNV, roughness), 0).rg;
        const float3 specular = prefiltered * (F0 * brdf.x + brdf.y);

        ambient = (kD * diffuse + specular) * uEnvironment.Params.x;
    }
    else
    {
        // Gradient fallback.
        ambient = albedo * lerp(uEnvironment.GroundColor.rgb, uEnvironment.ZenithColor.rgb, N.y * 0.5 + 0.5) * uEnvironment.Params.x;
    }

    // SSAO
    const float2 screenUv = input.Position.xy * uSsao.InvSize.xy;
    const float ssao = uSsaoTex.SampleLevel(uSsaoSampler, screenUv, 0);
    const float ssaoFactor = lerp(1.0, ssao, saturate(uSsao.Params.w));

    float3 outColor = ambient * materialAO * ssaoFactor + Lo + emissive;
    return float4(outColor, 1.0);
}
