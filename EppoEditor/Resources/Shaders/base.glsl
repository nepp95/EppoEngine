#define MAX_NUM_OF_LIGHTS 8

// Descriptor Set 0
layout(set = 0, binding = 0) uniform Camera
{
    mat4 View;
    mat4 Projection;
    mat4 ViewProjection;
    vec4 Position;
} uCamera;

struct Light
{
    mat4 View[6];
    vec4 Position;
    vec4 Color;
};

layout(set = 0, binding = 1) uniform Lights
{
    mat4 Projection;
    Light Lights[MAX_NUM_OF_LIGHTS];
    int NumLights;
} uLights;

layout(set = 0, binding = 2) uniform samplerCube uShadowMap;

// Descriptor Set 1
layout(set = 1, binding = 0) uniform sampler2D uMaterialTex[];
