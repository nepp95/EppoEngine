struct Input
{
    float4 Position : SV_Position;
    float3 ViewNormal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
};

float4 Main(Input input) : SV_Target
{
    return float4(normalize(input.ViewNormal) * 0.5 + 0.5, 1.0);
}
