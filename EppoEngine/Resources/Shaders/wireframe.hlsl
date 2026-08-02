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
    float4x4 InverseViewProjection;
    float4 Position;
    float NearClip;
    float FarClip;
};
ConstantBuffer<Camera> uCamera : register(b1, space0);

StructuredBuffer<float4x4> uInstanceTransforms : register(t0, space0);

Texture2D<float> uSceneDepth : register(t2, space0);

struct Varyings
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
};

Varyings VSMain(Input input)
{
    Varyings output;

    float4x4 instanceTransform = uInstanceTransforms[uPC.InstanceOffset + input.InstanceID];
    float4x4 worldTransform = mul(instanceTransform, uPC.Transform);
    float4 worldPos = mul(worldTransform, float4(input.Position, 1.0));

    output.Position = mul(uCamera.Projection, mul(uCamera.View, float4(worldPos.xyz, 1.0)));

    output.Normal = input.Normal;
    output.TexCoord = input.TexCoord;
    output.Tangent = input.Tangent;

    return output;
}

float4 PSMain(Varyings input) : SV_Target
{
    const float sceneDepth = uSceneDepth.Load(int3(input.Position.xy, 0));
    if (input.Position.z > sceneDepth + 0.0001f)
        discard;

    return uPC.WireframeColor;
}
