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

	mat4 rotView = mat4(mat3(uCamera.View));
	vec4 clipPos = uCamera.Projection * rotView * vec4(outPosition, 1.0);

	gl_Position = clipPos.xyww;
}

#stage frag
#version 450

#include "Includes/base.glsl"

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outFragColor;

void main()
{
	vec3 environmentColor = texture(uEnvironmentMap, inPosition).rgb;

	// HDR tonemap
	environmentColor = environmentColor / (environmentColor + vec3(1.0));
	
	outFragColor = vec4(environmentColor, 1.0);
}
