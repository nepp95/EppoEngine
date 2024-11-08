#stage vert
#version 450

layout(location = 0) out vec2 oUV;

void main()
{
	oUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(oUV * 2.0f - 1.0f, 0.0f, 1.0f);
}

#stage frag
#version 450

layout(location = 0) in vec2 iUV;

layout(binding = 0) uniform sampler2D texComposite;

layout(location = 0) out vec4 oFragColor;

void main()
{
	oFragColor = texture(texComposite, iUV);
}
