#include "Includes/platform.hlsli"

struct PushConstants
{
	float4x4 Transform;
	float4 WireframeColor;
	uint InstanceOffset;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space1);

float4 Main() : SV_Target
{
	return uPC.WireframeColor;
}
