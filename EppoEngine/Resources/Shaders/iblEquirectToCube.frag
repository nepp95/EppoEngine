#include "Includes/lighting.hlsli"
#include "Includes/platform.hlsli"

Texture2D uEquirect : register(t0, space0);
SamplerState uSampler : register(s0, space0);

struct PushConstants
{
	float Roughness;
	float EnvMapSize;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

struct Input
{
	float4 Position : SV_Position;
	float3 LocalDir : TEXCOORD0;
};

float4 Main(Input input) : SV_Target
{
	float3 dir = normalize(input.LocalDir);
	float2 uv;
	uv.x = atan2(dir.z, dir.x) / (2.0 * PI) + 0.5;
	uv.y = 0.5 - asin(clamp(dir.y, -1.0, 1.0)) / PI;
	return float4(uEquirect.Sample(uSampler, uv).rgb, 1.0);
}
