#include "Includes/fullscreen.hlsli"

Texture2D uTexture : register(t0, space0);
SamplerState uSampler : register(s0, space0);

FullscreenVaryings VSMain(uint vertexID : SV_VertexID)
{
    return BuildFullscreenTriangleVertex(vertexID);
}

float4 PSMain(FullscreenVaryings input) : SV_Target
{
    return uTexture.Sample(uSampler, input.TexCoord);
}
