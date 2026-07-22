struct Output
{
	float4 Position : SV_Position;
	float2 NDC : TEXCOORD0;
};

Output Main(uint vertexID : SV_VertexID)
{
	Output output;

	float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
	output.NDC = uv * 2.0 - 1.0;
	output.Position = float4(output.NDC, 1.0, 1.0);

	return output;
}
