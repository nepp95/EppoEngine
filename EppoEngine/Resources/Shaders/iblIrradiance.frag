#include "Includes/lighting.hlsli"
#include "Includes/platform.hlsli"

TextureCube uEnvMap : register(t0, space0);
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
	const float3 N = normalize(input.LocalDir);

	float3 up = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
	const float3 right = normalize(cross(up, N));
	up = cross(N, right);

	float3 irradiance = float3(0.0, 0.0, 0.0);
	uint sampleCount = 0;
	const float delta = 0.025;
	for (float phi = 0.0; phi < 2.0 * PI; phi += delta)
	{
		for (float theta = 0.0; theta < 0.5 * PI; theta += delta)
		{
			const float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			const float3 sampleDir = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
			irradiance += uEnvMap.SampleLevel(uSampler, sampleDir, 0).rgb * cos(theta) * sin(theta);
			sampleCount++;
		}
	}

	return float4(PI * irradiance / float(sampleCount), 1.0);
}
