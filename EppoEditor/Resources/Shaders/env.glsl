#stage vert
#version 450

#include "Includes/base.glsl"

#extension GL_EXT_multiview : enable

vec3 vertices[36] = vec3[]
(
	vec3(-1.0, -1.0, -1.0), vec3(-1.0, 1.0, -1.0), vec3(1.0, -1.0, -1.0), vec3(-1.0, 1.0, -1.0), vec3(1.0, 1.0, -1.0), vec3(1.0, -1.0, -1.0),
	vec3(1.0, -1.0, -1.0),  vec3(1.0, 1.0, -1.0),  vec3(1.0, -1.0, 1.0),  vec3(1.0, 1.0, -1.0),  vec3(1.0, 1.0, 1.0),  vec3(1.0, -1.0, 1.0),
	vec3(1.0, -1.0, 1.0),   vec3(1.0, 1.0, 1.0),   vec3(-1.0, -1.0, 1.0), vec3(1.0, 1.0, 1.0),   vec3(-1.0, 1.0, 1.0), vec3(-1.0, -1.0, 1.0),
	vec3(-1.0, -1.0, 1.0),  vec3(-1.0, 1.0, 1.0),  vec3(-1.0, -1.0, -1.0),vec3(-1.0, 1.0, 1.0),  vec3(-1.0, 1.0, -1.0),vec3(-1.0, -1.0, -1.0),
	vec3(-1.0, 1.0, -1.0),  vec3(-1.0, 1.0, 1.0),  vec3(1.0, 1.0, -1.0),  vec3(-1.0, 1.0, 1.0),  vec3(1.0, 1.0, 1.0),  vec3(1.0, 1.0, -1.0),
	vec3(-1.0, -1.0, 1.0),  vec3(-1.0, -1.0, -1.0),vec3(1.0, -1.0, 1.0),  vec3(-1.0, -1.0, -1.0),vec3(1.0, -1.0, -1.0),vec3(1.0, -1.0, 1.0)
);

layout(location = 0) out vec3 outPosition;

void main()
{
	outPosition = vertices[gl_VertexIndex];
	gl_Position = uEnvironment.Projection * uEnvironment.View[gl_ViewIndex] * vec4(vertices[gl_VertexIndex], 1.0);
}

#stage frag
#version 450

#include "Includes/constants.glsl"
#include "Includes/utility.glsl"
#include "Includes/base.glsl"

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outFragColor;

void main()
{
	vec2 uv = SampleSphericalMap(normalize(inPosition));
	vec3 color = texture(uEquirectangularMap, uv).rgb;

	outFragColor = vec4(color, 1.0);
}
