#ifndef FULLSCREEN_HLSLI
#define FULLSCREEN_HLSLI

// Shared vertex stage for full-screen passes: one oversized triangle covering the screen from three
// vertexIDs, with a UV that spans [0,1] across the viewport.

struct FullscreenVaryings
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

FullscreenVaryings BuildFullscreenTriangleVertex(uint vertexID)
{
    FullscreenVaryings output;
    output.TexCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.Position = float4(output.TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

#endif
