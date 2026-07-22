#include "Includes/platform.hlsli"

struct Input
{
	float2 Position : POSITION0;
	float2 UV : TEXCOORD0;
	float4 Color : COLOR0;
};

struct PushConstants
{
	float2 Scale;
	float2 Translate;
};
PUSH_CONSTANTS
ConstantBuffer<PushConstants> uPC : register(b0, space1);

struct Output
{
	float4 Position : SV_Position;
	float2 UV : TEXCOORD0;
	float4 Color : COLOR0;
};

Output Main(Input input)
{
	Output output;
	output.Position.xy = input.Position.xy * uPC.Scale + uPC.Translate;
	output.Position.zw = float2(0, 1);
	output.UV = input.UV;
	output.Color = input.Color;
    
	return output;
}