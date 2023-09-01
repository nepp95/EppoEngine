#version 450

layout(location = 0) in float inIndex;
layout(location = 0) out vec4 outColor;

void main()
{
	float color = inIndex * 0.05;
	outColor = vec4(color, color, color, 1.0);
}
