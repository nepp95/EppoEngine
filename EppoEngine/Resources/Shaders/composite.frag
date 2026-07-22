struct Input
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

Texture2D uTexture : register(t0, space0);
SamplerState uSampler : register(s0, space0);

float4 Main(Input input) : SV_Target
{
	return uTexture.Sample(uSampler, input.TexCoord);
}
