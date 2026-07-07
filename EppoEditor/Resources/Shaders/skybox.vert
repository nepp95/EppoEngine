// Fullscreen triangle generated from the vertex id — no vertex buffer needed.
// Emitted at z = 1.0 (far plane) so the sky pass fills only background pixels
// under a LessOrEqual depth test.
struct Output
{
	float4 Position : SV_Position;
	float2 NDC : TEXCOORD0;
};

Output Main(uint vertexID : SV_VertexID)
{
	Output output;

	float2 uv = float2((vertexID << 1) & 2, vertexID & 2); // (0,0), (2,0), (0,2)
	output.NDC = uv * 2.0 - 1.0;                            // (-1,-1), (3,-1), (-1,3)
	output.Position = float4(output.NDC, 1.0, 1.0);

	return output;
}
