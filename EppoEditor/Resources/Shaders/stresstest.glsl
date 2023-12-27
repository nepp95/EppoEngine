Stress testing code
vec2 uv = gl_FragCoord.xy / vec2(1280.0 / 720.0);

vec2 c = uv;
vec2 z = uv;

for (int i = 0; i < 50000; i++)
{
	z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
}

outColor = vec4(vec3(length(z) * 0.0001), 1.0);
