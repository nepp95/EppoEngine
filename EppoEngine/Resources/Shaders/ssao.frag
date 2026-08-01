struct Input
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

Texture2D uDepth : register(t0, space0);
Texture2D uNormal : register(t1, space0);
SamplerState uSampler : register(s0, space0);

struct Camera
{
	float4x4 View;
	float4x4 Projection;
	float4x4 ViewProjection;
	float4x4 InverseViewProjection;
	float4 Position;
};
ConstantBuffer<Camera> uCamera : register(b1, space0);

static const uint s_KernelSize = 32;
struct Ssao
{
    float4 Kernel[s_KernelSize];
    float4 Params;
    float4 InvSize;
};
ConstantBuffer<Ssao> uSsao : register(b2, space0);

float3 ReconstructViewPosition(float2 uv, float depth)
{
    const float2 ndcXY = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    float4 world = mul(uCamera.InverseViewProjection, float4(ndcXY, depth, 1.0));
    world /= world.w;
    return mul(uCamera.View, world).xyz;
}

float Main(Input input) : SV_Target
{
	const float depth = uDepth.SampleLevel(uSampler, input.TexCoord, 0);
	if (depth >= 1.0)
	    return 1.0;

	const float3 viewPos = ReconstructViewPosition(input.TexCoord, depth);
	const float3 normal = normalize(uNormal.SampleLevel(uSampler, input.TexCoord, 0).xyz * 2.0 - 1.0);

    // Stable per pixel rotation from integer pixel coords (interleaved gradient hash)
	const float2 px = input.Position.xy;
	const float angle = frac(52.9829189 * frac(dot(px, float2(0.06711056, 0.00583715)))) * 6.2831853;
	float3 randomVector = float3(cos(angle), sin(angle), 0.0);
	float3 tangent = randomVector - normal * dot(randomVector, normal);

	if (dot(tangent, tangent) < 1e-6)
	    tangent = abs(normal.x) < 0.9 ? float3(1, 0, 0) : float3(0, 1, 0);

    tangent = normalize(tangent);
    const float3 bitangent = cross(normal, tangent);
    const float3x3 tbn = float3x3(tangent, bitangent, normal);

    const float radius = uSsao.Params.x;
    const float bias = uSsao.Params.y;

    float occlusion = 0.0;
    float validSamples = 0.0;
    for (uint i = 0; i < s_KernelSize; i++)
    {
        float3 samplePos = viewPos + mul(uSsao.Kernel[i].xyz, tbn) * radius;

        float4 clipped = mul(uCamera.Projection, float4(samplePos, 1.0));
        clipped /= clipped.w;
        const float2 sampleUv = clipped.xy * float2(0.5, -0.5) + 0.5;
        if (any(sampleUv < 0.0) || any(sampleUv > 1.0))
            continue;

        const float sampleDepth = uDepth.SampleLevel(uSampler, sampleUv, 0);
        if (sampleDepth >= 1.0)
            continue;
        validSamples += 1.0;

        const float sceneDepth = -ReconstructViewPosition(sampleUv, sampleDepth).z;
        const float kernelDepth = -samplePos.z;
        const float originDepth = -viewPos.z;

        const float blocked = sceneDepth <= kernelDepth - bias ? 1.0 : 0.0;
        const float rangeWeight = smoothstep(0.0, 1.0, radius / max(abs(originDepth - sceneDepth), 1e-4));
        occlusion += blocked * rangeWeight;
    }

    float ao = 1.0 - occlusion / max(validSamples, 1.0);
    return pow(saturate(ao), max(uSsao.Params.z, 0.01));
}
