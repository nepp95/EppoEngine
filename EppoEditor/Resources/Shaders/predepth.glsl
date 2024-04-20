#stage vert
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(binding = 1) uniform Transform
{
	mat4 Transform;
} uTransform;

layout(binding = 2) uniform DirectionalLight
{
	mat4 View;
	mat4 Projection;
	vec4 Direction;
	vec4 AmbientColor;
	vec4 DiffuseColor;
	vec4 SpecularColor;
} uDirectionalLight;

void main()
{
	gl_Position = uDirectionalLight.Projection * uDirectionalLight.View * uTransform.Transform * vec4(inPosition, 1.0);
}

#stage frag
#version 450

void main()
{
	
}
