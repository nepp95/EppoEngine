#ifndef IBL_HLSLI
#define IBL_HLSLI

float2 Hammersley(uint i, uint N)
{
	uint bits = i;
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float2(float(i) / float(N), float(bits) * 2.3283064365386963e-10);
}

float SourceMipLevel(uint sampleCount, float pdf, float envMapSize)
{
	const float sampleSolidAngle = 1.0 / (float(sampleCount) * max(pdf, 0.000001));
	const float texelSolidAngle = 4.0 * PI / (6.0 * envMapSize * envMapSize);
	return max(0.5 * log2(sampleSolidAngle / texelSolidAngle), 0.0);
}

float3 CosineSampleHemisphere(float2 Xi, float3 N)
{
	const float radius = sqrt(Xi.x);
	const float phi = 2.0 * PI * Xi.y;
	const float3 sampleDir = float3(radius * cos(phi), radius * sin(phi), sqrt(1.0 - Xi.x));
	const float3 up = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
	const float3 tangent = normalize(cross(up, N));
	const float3 bitangent = cross(N, tangent);
	return normalize(tangent * sampleDir.x + bitangent * sampleDir.y + N * sampleDir.z);
}

// GGX half-vector importance sample around N (tangent-space H lifted to world).
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	float a = roughness * roughness;
	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
	float3 up = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);
	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

#endif
