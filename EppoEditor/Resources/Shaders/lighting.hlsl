#include "Includes/fullscreen.hlsli"
#include "Includes/lighting.hlsli"

static const uint s_MaxPointLights = 16;
static const uint s_CascadeCount = 4;
static const uint s_PointShadowFaceCount = s_MaxPointLights * 6;
static const float s_DirectionalInvMapSize = 1.0 / 2048.0;
static const float s_PointInvMapSize = 1.0 / 512.0;
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
    float ShadowDistance;
    uint3 Padding0;
    float4x4 PointLightViewProjections[s_PointShadowFaceCount];
    uint PointShadowMapIndex;
    uint PointShadowSamplerIndex;
    uint PointShadowLightCount;
    uint Padding1;
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
    float4 Direction;
    float4 Color;
};

struct Light
{
    float4 Position;
    float4 Color;
};

struct LightData
{
    DirectionalLight DirLight;
    Light Lights[s_MaxPointLights];
    uint NumLights;
    uint HasDirLight;
};
ConstantBuffer<LightData> uLights : register(b3, space0);

struct Environment
{
    float4 ZenithColor;
    float4 HorizonColor;
    float4 GroundColor;
    float4 Params;
    uint4 IBL0;
    uint4 IBL1;
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

Texture2D uBaseColorMetalness : register(t0, space0);
Texture2D uNormalRoughness : register(t1, space0);
Texture2D uEmissionAO : register(t2, space0);
Texture2D uDepth : register(t3, space0);
Texture2D uSsaoTex : register(t4, space0);
SamplerState uSampler : register(s0, space0);

bool SampleDirectionalCascade(const uint cascade, const float3 worldPosition, const float3 worldNormal, const float3 L, out float visibility)
{
    const Cascade cascadeData = uShadowDepth.Cascades[cascade];
    const float3 N = normalize(worldNormal);
    const float slope = 1.0 - saturate(dot(N, L));
    const float normalOffset = cascadeData.WorldUnitsPerTexel * uShadowDepth.NormalBiasTexels * slope;
    const float4 lightClip = mul(cascadeData.LightViewProjection, float4(worldPosition + N * normalOffset, 1.0));
    
    if (lightClip.w <= 0.0)
        return false;
    
    const float3 lightNdc = lightClip.xyz / lightClip.w;
    if (lightNdc.z < 0.0 || lightNdc.z > 1.0)
        return false;
    
    const float2 uv = lightNdc.xy * float2(0.5, -0.5) + 0.5;
    const float border = s_DirectionalInvMapSize * 1.5;
    if (any(uv < border) || any(uv > 1.0 - border))
        return false;
    
    Texture2DArray<float> shadowMap = ResourceDescriptorHeap[uShadowDepth.ShadowMapIndex];
    SamplerState shadowSampler = SamplerDescriptorHeap[uShadowDepth.ShadowSamplerIndex];
    
    const float compareDepth = lightNdc.z - uShadowDepth.DepthBiasTexels * s_DirectionalInvMapSize;
    float visibleSamples = 0.0;
    
    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            const float3 sampleUv = float3(uv + float2(x, y) * s_DirectionalInvMapSize, cascade);
            const float storedDepth = shadowMap.SampleLevel(shadowSampler, sampleUv, 0);
            visibleSamples += compareDepth <= storedDepth ? 1.0 : 0.0;
        }
    }
    
    visibility = visibleSamples / 9.0;
    return true;
}

float CalcShadowFactor(const float3 worldPosition, const float3 worldNormal, const float3 L)
{
    const float viewDepth = -mul(uCamera.View, float4(worldPosition, 1.0)).z;
    if (viewDepth <= 0.0 || viewDepth > uShadowDepth.ShadowDistance)
        return 1.0;
    
    uint cascade = 0;
    while (cascade + 1 < s_CascadeCount && viewDepth > uShadowDepth.Cascades[cascade].SplitDistance)
    {
        cascade++;
    }
    
    float visibility = 1.0;
    bool valid = SampleDirectionalCascade(cascade, worldPosition, worldNormal, L, visibility);
    
    while (!valid && cascade + 1 < s_CascadeCount)
    {
        cascade++;
        valid = SampleDirectionalCascade(cascade, worldPosition, worldNormal, L, visibility);
    }
    
    if (!valid)
        return 1.0;
    
    if (cascade + 1 < s_CascadeCount)
    {
        const Cascade cascadeData = uShadowDepth.Cascades[cascade];
        if (viewDepth >= cascadeData.TransitionStart)
        {
            float nextVisibility = 1.0;
            if (SampleDirectionalCascade(cascade + 1, worldPosition, worldNormal, L, nextVisibility))
            {
                const float blend = saturate((viewDepth - cascadeData.TransitionStart) / max(cascadeData.SplitDistance - cascadeData.TransitionStart, 0.0001));
                visibility = lerp(visibility, nextVisibility, blend);
            }
        }
    }
        
    return visibility;
}

static const float3 s_PcfOffsets[8] =
{ 
    float3(1.0, 1.0, 1.0),  float3(1.0, -1.0, -1.0), float3(-1.0, 1.0, -1.0), float3(-1.0, -1.0, 1.0),
    float3(1.0, 1.0, -1.0), float3(1.0, -1.0, 1.0),  float3(-1.0, 1.0, 1.0),  float3(-1.0, -1.0, -1.0),
};

float CalcPointShadowFactor(const uint index, const float3 worldPosition, const float3 worldNormal, const float3 L)
{
    if (index >= uShadowDepth.PointShadowLightCount)
        return 1.0;
    
    const Light light = uLights.Lights[index];
    const float range = light.Position.w;
    const float3 lightToFragment = worldPosition - light.Position.xyz;
    const float distanceToFragment = length(lightToFragment);
    if (distanceToFragment >= range)
        return 1.0;
    
    const float texelWorldSize = 2.0 * distanceToFragment * s_PointInvMapSize;
    const float3 N = normalize(worldNormal);
    const float slope = 1.0 - saturate(dot(N, L));
    const float3 samplePosition = worldPosition + N * texelWorldSize * uShadowDepth.NormalBiasTexels * slope;
    const float3 fragmentToLight = samplePosition - light.Position.xyz;
    const float compareDepth = length(fragmentToLight) / range - uShadowDepth.DepthBiasTexels * texelWorldSize / range;
    
    TextureCubeArray<float> shadowMap = ResourceDescriptorHeap[uShadowDepth.PointShadowMapIndex];
    SamplerState shadowSampler = SamplerDescriptorHeap[uShadowDepth.PointShadowSamplerIndex];
    
    float visibleSamples = 0.0;
    [unroll]
    for (uint tap = 0; tap < 8; tap++)
    {
        const float3 tapDirection = fragmentToLight + s_PcfOffsets[tap] * texelWorldSize;
        const float storedDepth = shadowMap.SampleLevel(shadowSampler, float4(tapDirection, index), 0);
        visibleSamples += compareDepth <= storedDepth ? 1.0 : 0.0;
    }
    
    return visibleSamples / 8.0;
}

float3 ReconstructWorldPosition(const float2 uv, const float depth)
{
    const float2 ndcXY =
        uv * float2(2.0, -2.0) +
        float2(-1.0, 1.0);

    float4 worldPosition = mul(
        uCamera.InverseViewProjection,
        float4(ndcXY, depth, 1.0)
    );

    return worldPosition.xyz / worldPosition.w;
}

FullscreenVaryings VSMain(uint vertexID : SV_VertexID)
{
    return BuildFullscreenTriangleVertex(vertexID);
}

float4 PSMain(FullscreenVaryings input) : SV_Target
{
    const float depth = uDepth.SampleLevel(uSampler, input.TexCoord, 0).r;

    if (depth >= 1.0)
        return float4(0.0, 0.0, 0.0, 1.0);

    const float4 g0 = uBaseColorMetalness.SampleLevel(uSampler, input.TexCoord, 0);
    const float4 g1 = uNormalRoughness.SampleLevel(uSampler, input.TexCoord, 0);
    const float4 g2 = uEmissionAO.SampleLevel(uSampler, input.TexCoord, 0);

    const float3 albedo = g0.rgb;
    const float metallic = saturate(g0.a);
    const float3 N = normalize(g1.rgb * 2.0 - 1.0);
    const float roughness = saturate(g1.a);
    const float3 emissive = g2.rgb;
    const float materialAO = saturate(g2.a);
    const float3 worldPosition = ReconstructWorldPosition(input.TexCoord, depth);
    const float3 V = normalize(uCamera.Position.xyz - worldPosition);

    // Directional lighting
    float3 Lo = float3(0.0, 0.0, 0.0);

    if (uLights.HasDirLight)
    {
        const float3 L = normalize(-uLights.DirLight.Direction.xyz);
        const float3 radiance = uLights.DirLight.Color.rgb * uLights.DirLight.Color.a;

        Lo += CalcShadowFactor(worldPosition, N, L) * BRDF(albedo, L, V, N, metallic, roughness, radiance);
    }

    // Point lighting
    const uint lightCount = min(uLights.NumLights, s_MaxPointLights);

    for (uint i = 0; i < lightCount; i++)
    {
        const Light light = uLights.Lights[i];
        const float3 toLight = light.Position.xyz - worldPosition;
        const float distanceSquared = max(dot(toLight, toLight), 0.0001);
        const float range = light.Position.w;
        const float distance = sqrt(distanceSquared);
        if (distance >= range)
            continue;
        const float3 L = toLight * rsqrt(distanceSquared);
        const float normalizedDistance = distance / range;
        const float squaredDistance = normalizedDistance * normalizedDistance;
        const float rangeWindow = saturate(1.0 - squaredDistance * squaredDistance);
        const float attenuation = rangeWindow * rangeWindow / distanceSquared;
        const float3 radiance = light.Color.rgb * light.Color.a * attenuation;

        Lo += CalcPointShadowFactor(i, worldPosition, N, L) * BRDF(albedo, L, V, N, metallic, roughness, radiance);
    }

    // Image-based ambient lighting when an environment has been baked.
    const float dotNV = max(dot(N, V), 0.0);
    float3 ambient;

    if (uEnvironment.Params.y > 0.5)
    {
        TextureCube irradianceMap = ResourceDescriptorHeap[uEnvironment.IBL0.y];
        TextureCube prefilterMap = ResourceDescriptorHeap[uEnvironment.IBL0.z];
        Texture2D brdfLut = ResourceDescriptorHeap[uEnvironment.IBL0.w];
        SamplerState iblSampler = SamplerDescriptorHeap[uEnvironment.IBL1.x];

        const float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
        const float3 Fmax = max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0);
        const float3 F = F0 + (Fmax - F0) * pow(1.0 - dotNV, 5.0);
        const float3 kD = (1.0 - F) * (1.0 - metallic);
        const float3 irradiance = irradianceMap.SampleLevel(iblSampler, N, 0).rgb;
        const float3 diffuse = irradiance * albedo;
        const float3 R = reflect(-V, N);
        const float maxLod = 4.0;
        const float3 prefiltered = prefilterMap.SampleLevel(iblSampler, R, roughness * maxLod).rgb;

        const float2 brdf = brdfLut.SampleLevel(iblSampler, float2(dotNV, roughness), 0).rg;
        const float3 specular = prefiltered * (F0 * brdf.x + brdf.y);

        ambient = (kD * diffuse + specular) * uEnvironment.Params.x;
    }
    else
    {
        ambient = albedo * lerp(uEnvironment.GroundColor.rgb, uEnvironment.ZenithColor.rgb, N.y * 0.5 + 0.5) * uEnvironment.Params.x;
    }

    const float ssao = uSsaoTex.SampleLevel(uSampler, input.TexCoord, 0).r;
    const float ssaoFactor = lerp(1.0, ssao, saturate(uSsao.Params.w));
    const float3 outColor = ambient * materialAO * ssaoFactor + Lo + emissive;

    return float4(outColor, 1.0);
}
