#include "Includes/platform.hlsli"

struct Input
{
    float4 Position : SV_Position;
};

struct PushConstants
{
	float4x4 Transform;
	float4 WireframeColor;
	uint InstanceOffset;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space1);

Texture2D<float> uSceneDepth : register(t2, space0);

float4 Main(Input input) : SV_Target
{
    const float sceneDepth = uSceneDepth.Load(int3(input.Position.xy, 0));
    if (input.Position.z > sceneDepth + 0.0001f)
        discard;

	return uPC.WireframeColor;
}
