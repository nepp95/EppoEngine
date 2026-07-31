struct Output
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

Output Main(uint vertexID : SV_VertexID)
{
	Output output;
	output.TexCoord = float2((vertexID << 1) & 2, vertexID & 2);
	output.Position = float4(output.TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	return output;
}
