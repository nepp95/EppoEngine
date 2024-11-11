#stage vert
#version 450

#include "base.glsl"

#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor;

layout(push_constant) uniform Transform
{
    layout(offset = 0) mat4 Transform;
	layout(offset = 64) int DiffuseMapIndex;
    layout(offset = 68) int NormalMapIndex;
    layout(offset = 72) int RoughnessMetallicMapIndex;
} uTransform;

void main()
{
    gl_Position = uLights.Projection * uLights.Lights[0].View[gl_ViewIndex] * uTransform.Transform * vec4(inPosition, 1.0);
}

#stage frag
#version 450

layout(push_constant) uniform Material
{
    layout(offset = 0) mat4 Transform;
	layout(offset = 64) int DiffuseMapIndex;
    layout(offset = 68) int NormalMapIndex;
    layout(offset = 72) int RoughnessMetallicMapIndex;
} uMaterial;

void main()
{

}
