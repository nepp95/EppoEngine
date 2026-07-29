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
	// Longitude around Y, latitude from -Y..+Y -> [0,1] UV.
	float2 uv = float2(atan2(dir.z, dir.x), asin(clamp(dir.y, -1.0, 1.0)));
	uv = uv / float2(2.0 * PI, PI) + 0.5;
	return float4(uEquirect.Sample(uSampler, uv).rgb, 1.0);
}
