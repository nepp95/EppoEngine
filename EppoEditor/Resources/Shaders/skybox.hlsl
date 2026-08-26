#include "Includes/fullscreen.hlsli"

// Procedural gradient sky. Reconstructs a world-space view ray per pixel and
// shades a vertical zenith->horizon->ground gradient. This is the fallback until
// an equirectangular HDR skybox is sampled (Environment.Params.y flags that).

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
ConstantBuffer<Camera> uCamera : register(b1, space0);

struct Environment
{
    float4 ZenithColor;
    float4 HorizonColor;
    float4 GroundColor;
    float4 Params; // x = ambient intensity, y = has skybox
    uint4 IBL0;    // x = env cube, y = irradiance, z = prefilter, w = BRDF LUT bindless indices
    uint4 IBL1;    // x = IBL sampler index
};
ConstantBuffer<Environment> uEnvironment : register(b3, space0);

Texture2D uSceneDepth : register(t0, space0);
SamplerState uDepthSampler : register(s0, space0);

FullscreenVaryings VSMain(uint vertexID : SV_VertexID)
{
    return BuildFullscreenTriangleVertex(vertexID);
}

float4 PSMain(FullscreenVaryings input) : SV_Target
{
    if (uSceneDepth.SampleLevel(uDepthSampler, input.TexCoord, 0).r < 1.0)
        discard;

    const float2 ndc = input.TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0);

    // Reconstruct the world-space ray direction through this pixel by unprojecting
    // the near and far clip points and taking their difference.
    float4 worldNear = mul(uCamera.InverseViewProjection, float4(ndc, 0.0, 1.0));
    float4 worldFar = mul(uCamera.InverseViewProjection, float4(ndc, 1.0, 1.0));
    float3 dir = normalize(worldFar.xyz / worldFar.w - worldNear.xyz / worldNear.w);

    if (uEnvironment.Params.y > 0.5)
    {
        TextureCube envMap = ResourceDescriptorHeap[uEnvironment.IBL0.x];
        SamplerState samp = SamplerDescriptorHeap[uEnvironment.IBL1.x];
        return float4(envMap.SampleLevel(samp, dir, 0).rgb, 1.0);
    }

    float t = dir.y;
    float3 color;
    if (t > 0.0)
        color = lerp(uEnvironment.HorizonColor.rgb, uEnvironment.ZenithColor.rgb, pow(saturate(t), 0.5));
    else
        color = lerp(uEnvironment.HorizonColor.rgb, uEnvironment.GroundColor.rgb, pow(saturate(-t), 0.5));

    return float4(color, 1.0);
}
