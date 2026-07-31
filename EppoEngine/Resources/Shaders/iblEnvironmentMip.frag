#include "Includes/platform.hlsli"

struct PushConstants
{
	uint SourceIndex;
	uint SamplerIndex;
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
	TextureCube source = ResourceDescriptorHeap[NonUniformResourceIndex(uPC.SourceIndex)];
	SamplerState sourceSampler = SamplerDescriptorHeap[NonUniformResourceIndex(uPC.SamplerIndex)];
	return float4(source.SampleLevel(sourceSampler, normalize(input.LocalDir), 0.0).rgb, 1.0);
}
