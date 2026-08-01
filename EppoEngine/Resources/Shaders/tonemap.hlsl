#include "Includes/fullscreen.hlsli"
#include "Includes/lighting.hlsli"
#include "Includes/platform.hlsli"

struct PushConstants
{
    float Exposure;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

Texture2D uTexture : register(t0, space0);
SamplerState uSampler : register(s0, space0);

FullscreenVaryings VSMain(uint vertexID : SV_VertexID)
{
    return BuildFullscreenTriangleVertex(vertexID);
}

float4 PSMain(FullscreenVaryings input) : SV_Target
{
    const float3 hdr = uTexture.Sample(uSampler, input.TexCoord).rgb * uPC.Exposure;
    float3 color = ACESFilm(hdr);
    color = pow(color, float3(0.4545, 0.4545, 0.4545));
    return float4(color, 1.0);
}
