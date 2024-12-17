#stage vert
#version 450

#include "base.glsl"

#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(push_constant) uniform PreDepth
{
	layout(offset = 0)  mat4 Transform;
    layout(offset = 64) int LightIndex;
} uPreDepth;

void main()
{
    gl_Position = uLights.Projection * uLights.Lights[uPreDepth.LightIndex].View[gl_ViewIndex] * uPreDepth.Transform * vec4(inPosition, 1.0);
}

#stage frag
#version 450

void main()
{
	float near = 0.1;
	float far = 50.0;

	float linearDepth = (2.0 * near) / (far + near - gl_FragCoord.z * (far - near));

	gl_FragDepth = linearDepth;
}
