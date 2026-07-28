// Procedural gradient sky. Reconstructs a world-space view ray per pixel and
// shades a vertical zenith->horizon->ground gradient. This is the fallback until
// an equirectangular HDR skybox is sampled (Environment.Params.y flags that).

struct Camera
{
	float4x4 View;
	float4x4 Projection;
	float4x4 ViewProjection;
	float4 Position;
	float4x4 InverseViewProjection;
};
ConstantBuffer<Camera> uCamera : register(b1, space0);

struct Environment
{
	float4 ZenithColor;
	float4 HorizonColor;
	float4 GroundColor;
	float4 Params; // x = ambient intensity, y = has skybox
};
ConstantBuffer<Environment> uEnvironment : register(b3, space0);

struct Input
{
	float4 Position : SV_Position;
	float2 NDC : TEXCOORD0;
};

float4 Main(Input input) : SV_Target
{
	// Reconstruct the world-space ray direction through this pixel by unprojecting
	// the near and far clip points and taking their difference.
	float4 worldNear = mul(uCamera.InverseViewProjection, float4(input.NDC, 0.0, 1.0));
	float4 worldFar = mul(uCamera.InverseViewProjection, float4(input.NDC, 1.0, 1.0));
	float3 dir = normalize(worldFar.xyz / worldFar.w - worldNear.xyz / worldNear.w);

	float t = dir.y;
	float3 color;
	if (t > 0.0)
		color = lerp(uEnvironment.HorizonColor.rgb, uEnvironment.ZenithColor.rgb, pow(saturate(t), 0.5));
	else
		color = lerp(uEnvironment.HorizonColor.rgb, uEnvironment.GroundColor.rgb, pow(saturate(-t), 0.5));

	return float4(color, 1.0);
}
