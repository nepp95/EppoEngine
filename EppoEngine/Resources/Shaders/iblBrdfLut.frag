#include "Includes/lighting.hlsli"
#include "Includes/ibl.hlsli"

struct Input
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

// Split-sum second sum. 2D RG16_FLOAT target, TexCoord = (dotNV, roughness), no texture inputs.
float4 Main(Input input) : SV_Target
{
	const float dotNV = input.TexCoord.x;
	const float roughness = max(input.TexCoord.y, 0.001);

	const float3 V = float3(sqrt(1.0 - dotNV * dotNV), 0.0, dotNV);
	const float3 N = float3(0.0, 0.0, 1.0);

	float A = 0.0;
	float B = 0.0;
	const uint sampleCount = 1024u;

	for (uint i = 0u; i < sampleCount; i++)
	{
		const float2 Xi = Hammersley(i, sampleCount);
		const float3 H = ImportanceSampleGGX(Xi, N, roughness);
		const float3 L = normalize(2.0 * dot(V, H) * H - V);

		const float dotNL = max(L.z, 0.0);
		const float dotNH = max(H.z, 0.0);
		const float dotVH = max(dot(V, H), 0.0);

		if (dotNL > 0.0)
		{
			// IBL geometry term: k = a/2 (not the direct-lighting (r+1)^2/8 in GSchlickSmithGGX).
			const float k = (roughness * roughness) / 2.0;
			const float G = (dotNL / (dotNL * (1.0 - k) + k)) * (dotNV / (dotNV * (1.0 - k) + k));
			const float GVis = G * dotVH / (dotNH * dotNV);
			const float Fc = pow(1.0 - dotVH, 5.0);
			A += (1.0 - Fc) * GVis;
			B += Fc * GVis;
		}
	}

	return float4(A / float(sampleCount), B / float(sampleCount), 0.0, 1.0);
}
