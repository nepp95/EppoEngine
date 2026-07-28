#include "Includes/lighting.hlsli"
#include "Includes/platform.hlsli"

struct Input
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

struct PushConstants
{
    float Exposure;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

Texture2D uTexture : register(t0, space0);
SamplerState uSampler : register(s0, space0);

float4 Main(Input input) : SV_Target
{
    const float3 hdr = uTexture.Sample(uSampler, input.TexCoord).rgb * uPC.Exposure;
    float3 color = ACESFilm(hdr);
    color = pow(color, float3(0.4545, 0.4545, 0.4545));
    return float4(color, 1.0);
}
