#stage vert
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(binding = 1) uniform Transform
{
	mat4 Transform;
} uTransform;

layout(binding = 2) uniform Environment
{
	mat4 LightView;
	mat4 LightProjection;
	mat4 LightViewProjection;
	vec3 LightPosition;
	vec3 LightColor;
} uEnvironment;

void main()
{
	gl_Position = uEnvironment.LightViewProjection * uTransform.Transform * vec4(inPosition, 1.0);
}

#stage frag
#version 450

void main()
{
	
}
