#include "Includes/lighting.hlsli"
#include "Includes/platform.hlsli"

struct Input
{
	float3 Position : POSITION0;
	float3 Normal : NORMAL0;
	float2 TexCoord : TEXCOORD0;
	float4 Tangent : TANGENT0;
	uint InstanceID : SV_InstanceID;
};

struct PushConstants
{
	uint DrawIndex;
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
ConstantBuffer<Camera> uCamera : register(b2, space0);

StructuredBuffer<float4x4> uInstanceTransforms : register(t0, space0);

struct DrawData
{
    float4x4 Transform;
    uint InstanceOffset;
    uint MaterialIndex;
};
StructuredBuffer<DrawData> uDrawData : register(t1, space0);

struct MaterialData
{
    int DiffuseMapIndex;
    int NormalMapIndex;
    int RoughMetMapIndex;
    int AOMapIndex;
    int EmissiveMapIndex;
    float4 BaseColor;
    float3 EmissiveFactor;
    float Metallic;
    float Roughness;
};
StructuredBuffer<MaterialData> uMaterialData : register(t2, space0);

struct Output
{
	float4 Position : SV_Position;
	float3 WorldPos : POSITION0;
	float3 Normal : NORMAL0;
	float2 TexCoord : TEXCOORD0;
	float4 WorldTangent : TANGENT0;
};

Output Main(Input input)
{
	Output output;

	DrawData draw = uDrawData[uPC.DrawIndex];

	const float4x4 instanceTransform = uInstanceTransforms[draw.InstanceOffset + input.InstanceID];
	const float4x4 worldTransform = mul(instanceTransform, draw.Transform);
	
	output.WorldPos = mul(worldTransform, float4(input.Position, 1.0)).xyz;
	output.Normal = mul((float3x3)worldTransform, input.Normal);
	output.Position = mul(uCamera.Projection, mul(uCamera.View, float4(output.WorldPos, 1.0)));
	output.TexCoord = input.TexCoord;
	output.WorldTangent = float4(mul((float3x3)worldTransform, input.Tangent.xyz), input.Tangent.w);

	return output;
}