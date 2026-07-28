#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

static const float PI = 3.14159265359;

// Normal distribution GGX
float DGGX(float dotNH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denominator = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    return (alpha2) / (PI * denominator * denominator);
}

// Geometry shadowing
float GSchlickSmithGGX(float dotNL, float dotNV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float GL = dotNL / (dotNL * (1.0 - k) + k);
    float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

// Fresnel
float3 FSchlick(float3 matColor, float cosTheta, float metallic)
{
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), matColor, metallic);
    float3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
    return F;
}

// Cook-Torrance BRDF for a single light. `radiance` is the incoming light
// (color * intensity * attenuation); the caller computes it per light.
float3 BRDF(float3 matColor, float3 L, float3 V, float3 N, float metallic, float roughness, float3 radiance)
{
    float3 H = normalize(V + L);
    float dotNV = clamp(dot(N, V), 0.0, 1.0);
    float dotNL = clamp(dot(N, L), 0.0, 1.0);
    float dotNH = clamp(dot(N, H), 0.0, 1.0);

    float3 color = float3(0.0, 0.0, 0.0);

    if (dotNL > 0.0)
    {
        // Clamp roughness for both D and G: raw roughness 0 makes DGGX 0/0.
        float rroughness = max(0.05, roughness);
        float D = DGGX(dotNH, rroughness);
        float G = GSchlickSmithGGX(dotNL, dotNV, rroughness);
        float3 F = FSchlick(matColor, dotNV, metallic);
        float3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);

        // Energy-conserving Lambertian diffuse: metals have no diffuse response,
        // and the fraction reflected specularly (F) is removed from the diffuse lobe.
        float3 kD = (1.0 - F) * (1.0 - metallic);
        color += (kD * matColor / PI + spec) * dotNL * radiance;
    }

    return color;
}

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

// Narkowicz
float3 ACESFilm(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

#endif