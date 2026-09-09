#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

static const float PI = 3.14159265359;

// Correct transform for normals under non-uniform scale (adjugate method).
// Orthonormal inputs pass through unchanged.
float3x3 InverseTranspose3x3(float3x3 m)
{
    float3 c0 = float3(m[0].x, m[1].x, m[2].x);
    float3 c1 = float3(m[0].y, m[1].y, m[2].y);
    float3 c2 = float3(m[0].z, m[1].z, m[2].z);

    float3 r0 = cross(c1, c2);
    float3 r1 = cross(c2, c0);
    float3 r2 = cross(c0, c1);

    float invDet = 1.0 / dot(c0, r0);
    return transpose(float3x3(r0, r1, r2)) * invDet;
}

#endif
