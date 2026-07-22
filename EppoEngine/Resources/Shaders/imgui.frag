struct Input
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

Texture2D uTexture : register(t0, space0);
SamplerState uSampler : register(s0, space0);

float4 Main(Input input) : SV_Target
{
    float4 sample = uTexture.Sample(uSampler, input.UV);
    return input.Color * sample;
}