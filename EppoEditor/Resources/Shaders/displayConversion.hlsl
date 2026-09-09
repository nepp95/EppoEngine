#include "Includes/fullscreen.hlsli"

Texture2D uTexture : register(t0, space0);
SamplerState uSampler : register(s0, space0);

FullscreenVaryings VSMain(uint vertexID : SV_VertexID)
{
    return BuildFullscreenTriangleVertex(vertexID);
}

float4 PSMain(FullscreenVaryings input) : SV_Target
{
    const float3 linea = saturate(uTexture.Sample(uSampler, input.TexCoord).rgb);
    const float3 low = linea * 12.92;
    const float3 high = 1.055 * pow(linea, 1.0 / 2.4) - 0.055;
    return float4(lerp(low, high, step(0.0031308, linea)), 1.0);
}