#stage vert
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outFragPosition;
layout(location = 2) out vec3 outViewPosition;

layout(set = 1, binding = 0) uniform Camera
{
	mat4 ViewMatrix;
	mat4 ViewProjection;
} uCamera;

layout(push_constant) uniform Transform
{
	layout(offset = 0) mat4 Transform;
} uTransform;

void main()
{
	outNormal = inNormal;
	outFragPosition = vec3(uTransform.Transform * vec4(inPosition, 1.0));

	vec4 worldPosition = uTransform.Transform * vec4(inPosition, 1.0);
	outViewPosition = vec3(uCamera.ViewMatrix * vec4(worldPosition.xyz, 1.0));

	gl_Position = uCamera.ViewProjection * uTransform.Transform * vec4(inPosition, 1.0);
}

#stage frag
#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inFragPosition;
layout(location = 2) in vec3 inViewPosition;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform Environment
{
	vec3 LightPosition;
	vec3 LightColor;
} uEnvironment;

layout(push_constant) uniform Material
{
	layout(offset = 64) vec3 AlbedoColor;
	layout(offset = 80) float Roughness;
} uMaterial;

void main()
{
	// using the Phong Reflection Model: https://en.wikipedia.org/wiki/Phong_reflection_model

	// Ambient
	// Ia = Ka * I
	// Ia = ambient intensity
	// Ka = ambient intensity coefficient
	// I = light intensity
	float ambientIntensity = 0.2;
	vec3 ambient = ambientIntensity * uEnvironment.LightColor;

	// Diffuse
	// Id = Kd * I * max(dot(N, L), 0)
	// Id = diffuse intensity
	// Kd = diffuse intensity coefficient
	// I = light intensity
	// N = normalized surface normal vector
	// L = normalized light direction vector
	vec3 nNormal = normalize(inNormal);
	vec3 nLightDirection = normalize(uEnvironment.LightPosition - inFragPosition);

	vec3 diffuseIntensity = uEnvironment.LightColor * max(dot(nNormal, nLightDirection), 0.0); // * roughness
	vec3 diffuse = diffuseIntensity * uEnvironment.LightColor;

	// Specular
	// Is = Ks * I * (max(dot(R, V), 0)n
	// Is = specular intensity
	// Ks = specular intensity coefficient
	// I = light intensity
	// R = normalized reflected light direction vector
	// V = normalized view direction vector
	// n = shininess coefficient
	vec3 nViewDirection = normalize(inViewPosition - inFragPosition);
	vec3 nReflectDirection = normalize(reflect(-nLightDirection, nNormal));
	
	float specularIntensity = 0.5;
	vec3 specular = specularIntensity * uEnvironment.LightColor * max(dot(nViewDirection, nReflectDirection), 0.0);

	// Output
	vec3 result = (ambient + diffuse + specular) * uMaterial.AlbedoColor;
	outColor = vec4(result, 1.0);
}
