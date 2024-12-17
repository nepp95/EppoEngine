// From: https://learnopengl.com/PBR/IBL/Diffuse-irradiance
vec2 SampleSphericalMap(vec3 v)
{
	vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
	uv *= invAtan;
	uv += 0.5;

	return uv;
}
