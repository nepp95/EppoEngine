#include "Includes/platform.hlsli"
#include "Includes/ibl_cube.hlsli"

Output Main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	return BuildCubeFaceVertex(vertexID, instanceID);
}
