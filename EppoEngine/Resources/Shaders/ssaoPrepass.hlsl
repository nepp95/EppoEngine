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
    float4x4 InverseViewProjection;
    float4 Position;
};
ConstantBuffer<Camera> uCamera : register(b1, space0);

StructuredBuffer<float4x4> uInstanceTransforms : register(t0, space0);

struct DrawData
{
    float4x4 Transform;
    uint InstanceOffset;
    uint MaterialIndex;
};
StructuredBuffer<DrawData> uDrawData : register(t1, space0);

struct Varyings
{
    float4 Position : SV_Position;
    float3 ViewNormal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
};

Varyings VSMain(Input input)
{
    Varyings output;
    DrawData draw = uDrawData[uPC.DrawIndex];

    const float4x4 instanceTransform = uInstanceTransforms[draw.InstanceOffset + input.InstanceID];
    const float4x4 worldTransform = mul(instanceTransform, draw.Transform);
    const float3 worldPos = mul(worldTransform, float4(input.Position, 1.0)).xyz;
    const float3 worldNormal = normalize(mul(InverseTranspose3x3((float3x3)worldTransform), input.Normal));

    output.Position = mul(uCamera.ViewProjection, float4(worldPos, 1.0));
    output.ViewNormal = normalize(mul((float3x3)uCamera.View, worldNormal));
    output.TexCoord = input.TexCoord;
    output.Tangent = input.Tangent;

    return output;
}

float4 PSMain(Varyings input) : SV_Target
{
    return float4(normalize(input.ViewNormal) * 0.5 + 0.5, 1.0);
}
