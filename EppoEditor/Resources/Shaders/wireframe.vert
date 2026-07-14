#include "Includes/platform.hlsli"

struct Input
{
	float3 Position : POSITION0;
	float3 Normal : NORMAL0;
	float2 TexCoord : TEXCOORD0;
	uint InstanceID : SV_InstanceID;
};

struct PushConstants
{
	float4x4 Transform;
	float4 WireframeColor;
	uint InstanceOffset;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space0);

struct Camera
{
	float4x4 View;
	float4x4 Projection;
	float4x4 ViewProjection;
	float3 Position;
};
ConstantBuffer<Camera> uCamera : register(b1, space0);

StructuredBuffer<float4x4> uInstanceTransforms : register(t0, space0);

struct Output
{
	float4 Position : SV_Position;
	float3 Normal : NORMAL0;
	float2 TexCoord : TEXCOORD0;
};

Output Main(Input input)
{
	Output output;

	float4x4 instanceTransform = uInstanceTransforms[uPC.InstanceOffset + input.InstanceID];
	float4x4 worldTransform = mul(instanceTransform, uPC.Transform);
	float4 worldPos = mul(worldTransform, float4(input.Position, 1.0));

	output.Position = mul(uCamera.Projection, mul(uCamera.View, float4(worldPos.xyz, 1.0)));

	// Reference Normal/TexCoord so the compiler keeps them as stage inputs and
	// the reflected vertex stride stays 32 bytes (matches the interleaved Vertex).
	output.Normal = input.Normal;
	output.TexCoord = input.TexCoord;

	return output;
}
