#ifndef IBL_CUBE_HLSLI
#define IBL_CUBE_HLSLI

// Shared vertex stage for the layered cube bakes (equirect->cube, irradiance, prefilter): one
// instance per face routed by SV_RenderTargetArrayIndex, per-face view ray from the face UB.
// Each pass declares its own push constants; this stage only needs the per-face matrices.

struct Faces
{
    float4x4 InvViewProjection[6];
};
ConstantBuffer<Faces> uFaces : register(b1, space0);

struct CubeFaceVaryings
{
    float4 Position : SV_Position;
    float3 LocalDir : TEXCOORD0;
    uint Layer : SV_RenderTargetArrayIndex; // routes this instance to its cube face
};

CubeFaceVaryings BuildCubeFaceVertex(uint vertexID, uint instanceID)
{
    CubeFaceVaryings output;
    output.Layer = instanceID;

    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    float2 ndc = uv * 2.0 - 1.0;
    output.Position = float4(ndc, 0.0, 1.0);

    // Cubemap texel rows have a top-left origin, opposite the framebuffer-space NDC Y used by this pass.
    float2 cubeNdc = float2(ndc.x, -ndc.y);
    float4x4 invVP = uFaces.InvViewProjection[instanceID];
    float4 nearPos = mul(invVP, float4(cubeNdc, 0.0, 1.0));
    float4 farPos = mul(invVP, float4(cubeNdc, 1.0, 1.0));
    output.LocalDir = farPos.xyz / farPos.w - nearPos.xyz / nearPos.w;
    return output;
}

#endif
