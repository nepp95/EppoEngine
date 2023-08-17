#version 450

layout(location = 0) in vec4 fColor;
layout(location = 1) in vec2 fTexCoord;
layout(location = 2) in float fTexIndex;

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D texSampler[32];

void main()
{
	outColor = texture(texSampler[int(fTexIndex)], fTexCoord) * fColor;

	if (outColor.a == 0)
		discard;
}
