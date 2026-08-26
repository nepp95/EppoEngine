# Remaining PBR Rendering Plan

Apply the following deltas in order. Compile and verify each stage before applying the next stage.

# Stage 1 — Complete material shadows and repair directional cascades

## 1. Change `shadowDepth.hlsl` to fetch materials through `DrawIndex`

File: `EppoEditor/Resources/Shaders/shadowDepth.hlsl`

### 1.1 Change the push constants

The current shadow push constants contain `Transform`, `InstanceOffset`, and the partially added `MaterialIndex`. Remove those fields and add the same `DrawIndex` used by `geometry.hlsl`.

```hlsl
struct PushConstants
{
    // REMOVE
    float4x4 Transform;
    uint InstanceOffset;
    uint MaterialIndex;
    //

    // ADD HERE
    uint DrawIndex;
    //
};
```

### 1.2 Add the `DrawData` layout before `MaterialData`

Add this immediately after `uInstanceTransforms` and before the currently added `MaterialData` declaration.

```hlsl
StructuredBuffer<float4x4> uInstanceTransforms : register(t0, space0);

// ADD HERE
struct DrawData
{
    float4x4 Transform;
    uint InstanceOffset;
    uint MaterialIndex;
};
StructuredBuffer<DrawData> uDrawData : register(t1, space0);
//
```

### 1.3 Change the material-buffer register

```hlsl
// CHANGE
StructuredBuffer<MaterialData> uMaterialData : register(t2, space0);
```

### 1.4 Fetch `DrawData` inside `VSMain`

Add `draw` immediately after `Varyings output`, then change the two transform-data lines:

```hlsl
Varyings VSMain(Input input)
{
    Varyings output;

    // ADD HERE
    const DrawData draw = uDrawData[uPC.DrawIndex];
    //

    const uint cascade = input.InstanceID % s_CascadeCount;
    const uint objectInstance = input.InstanceID / s_CascadeCount;

    // REMOVE
    const float4x4 instanceTransform = uInstanceTransforms[uPC.InstanceOffset + objectInstance];
    const float4x4 worldTransform = mul(instanceTransform, uPC.Transform);
    //

    // ADD HERE
    const float4x4 instanceTransform = uInstanceTransforms[draw.InstanceOffset + objectInstance];
    const float4x4 worldTransform = mul(instanceTransform, draw.Transform);
    //

    const float4 worldPosition = mul(worldTransform, float4(input.Position, 1.0));
}
```

### 1.5 Change the empty `PSMain` signature and body

Change `PSMain` so it reads the material through `uPC.DrawIndex`, samples base-colour alpha, and clips masked texels.

```hlsl
// CHANGE
void PSMain(Varyings input)
{
    // REMOVE
    // We have no color output with shadow depth
    //

    // ADD HERE
    const DrawData draw = uDrawData[uPC.DrawIndex];
    const MaterialData material = uMaterialData[draw.MaterialIndex];
    const uint alphaMode = material.Flags & 0x3u;

    if (alphaMode != 1u)
        return;

    float alpha = material.BaseColor.a;

    if (material.DiffuseMapIndex > -1)
    {
        Texture2D diffuseMap =
            ResourceDescriptorHeap[NonUniformResourceIndex(material.DiffuseMapIndex)];
        SamplerState diffuseSampler =
            SamplerDescriptorHeap[NonUniformResourceIndex(material.DiffuseSamplerIndex)];

        alpha *= diffuseMap.Sample(diffuseSampler, input.TexCoord).a;
    }

    clip(alpha - material.AlphaCutoff);
    //
}
```

## 2. Bind the draw and material buffers to the shadow pass

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Location: constructor input setup, immediately after the two shadow inputs.

```cpp
m_ShadowDepthPass->SetInput(0, 0, m_InstanceTransformsSB);
m_ShadowDepthPass->SetInput(0, 1, m_ShadowDepthUB);

// ADD HERE
m_ShadowDepthPass->SetInput(0, 1, m_DrawDataSB);
m_ShadowDepthPass->SetInput(0, 2, m_MaterialDataSB);
//
```

This supplies `b1`, `t1`, and `t2` using their separate HLSL resource namespaces.

## 3. Change `SceneRenderer::ShadowDepthPass` to DrawIndex

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

### 3.1 Change the local push-constant fields

```cpp
struct PC
{
    // REMOVE
    glm::mat4 Transform;
    uint32_t InstanceOffset;
    //

    // ADD HERE
    uint32_t DrawIndex;
    //
} pushConstants{};
```

### 3.2 Add the shadow draw index after the graphics-state reference

```cpp
auto& state = m_RenderCommandBuffer->GetGraphicsState();

// ADD HERE
uint32_t drawIndex = 0;
//

for (const auto& drawCmd : m_DrawCommands | std::views::values)
```

### 3.3 Remove the now-unused per-submesh push-constant assignments

```cpp
// REMOVE
pushConstants.Transform = submesh.LocalTransform;
pushConstants.InstanceOffset = drawCmd.InstanceOffset;
//
```

### 3.4 Add DrawIndex, blend exclusion, and pipeline selection inside the primitive loop

The increment occurs exactly once for every primitive and before the blend exclusion, preserving alignment with `m_DrawData`.

```cpp
for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
{
    // ADD HERE
    pushConstants.DrawIndex = drawIndex++;

    if (material->AlphaMode == MaterialAlphaMode::Blend)
        continue;

    state.pipeline = material->DoubleSided
        ? m_ShadowDepthDoubleSidedPipeline->GetPipeline()
        : m_ShadowDepthPass->GetPipeline()->GetPipeline();
    m_RenderCommandBuffer->CommitGraphicsState();
    //

    cmdList->setPushConstants(&pushConstants, sizeof(PC));

}
```

### 3.5 Assert alignment immediately before ending the pass

```cpp
// ADD HERE
EP_ASSERT(drawIndex == m_DrawData.size());
//

Renderer::EndRenderPass(m_RenderCommandBuffer);
```

## 4. Add the double-sided directional-shadow pipeline

File: `EppoEngine/Source/Renderer/SceneRenderer.h`

Location: next to `m_GeometryDoubleSidedPipeline`.

```cpp
Ref<Pipeline> m_GeometryDoubleSidedPipeline = nullptr;

// ADD HERE
Ref<Pipeline> m_ShadowDepthDoubleSidedPipeline = nullptr;
//
```

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Location: constructor, inside the `// Shadow Depth` block, after `m_ShadowDepthPass` is created.

```cpp
m_ShadowDepthPass = CreateRef<RenderPass>(renderPassSpec);

// ADD HERE
auto doubleSidedPipelineSpec = pipelineSpec;
doubleSidedPipelineSpec.CullMode = nvrhi::RasterCullMode::None;
m_ShadowDepthDoubleSidedPipeline = CreateRef<Pipeline>(
    doubleSidedPipelineSpec,
    framebuffer->GetFramebuffer(cascadeSubresources)->getFramebufferInfo()
);
//
```

## 5. Add material import/render regression tests

File: `EppoEngineTesting/Source/Renderer/Mesh.cpp`

Add tests after the glTF material-loading tests:

```cpp
// ADD HERE
TEST(Renderer, Mesh_GltfMaterial_ImportsAlphaModeCutoffAndDoubleSided)
{
    // Load a temporary glTF containing OPAQUE, MASK, and BLEND materials.
    // Require the imported enum, custom cutoff, and double-sided flag.
}
//
```

File: `EppoEngineTesting/Source/Renderer/SceneRendering.cpp`

Add tests after `SceneRenderer_DirectionalShadowDarkensReceiverAndSurvivesResize`:

```cpp
// ADD HERE
TEST(Renderer, SceneRenderer_AlphaMaskMatchesGeometryAndDirectionalShadow)
{
    // Render the same alpha mask through geometry and directional shadow coverage.
    // Require rejected texels to expose the background and admit directional light.
}

TEST(Renderer, SceneRenderer_DoubleSidedMaterialRendersAndCastsFromBothSides)
{
    // Render opposite sides of a double-sided primitive and its shadow.
    // Require visible, consistently lit faces and shadow coverage from both sides.
}
//
```

## 6. Change cascade padding into scale and transition data

File: `EppoEngine/Source/Renderer/SceneRenderer.h`

```cpp
struct Cascade
{
    glm::mat4 LightViewProjection;
    float SplitDistance;

    // REMOVE
    glm::vec3 Padding;
    //

    // ADD HERE
    float WorldUnitsPerTexel;
    float TransitionStart;
    float Padding;
    //
};
```

Apply the same field delta to the `Cascade` declarations in:

- `EppoEditor/Resources/Shaders/lighting.hlsl`
- `EppoEditor/Resources/Shaders/shadowDepth.hlsl`

```hlsl
struct Cascade
{
    float4x4 LightViewProjection;
    float SplitDistance;

    // ADD HERE
    float WorldUnitsPerTexel;
    float TransitionStart;
    float Padding;
    //
};
```

## 7. Remove the independent 100-unit shadow limit

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

```cpp
// REMOVE
constexpr float s_ShadowDistance = 100.0f;
//
```

Inside `FillShadowData`, remove the `ShadowDistance` initializer:

```cpp
m_ShadowDepthData = {
    .DepthBias = s_ShadowBias,
    .NormalBias = s_ShadowNormalBias,
    .InvMapSize = 1.0f / s_ShadowMapSize,

    // REMOVE
    .ShadowDistance = s_ShadowDistance,
    //
};
```

Change the far-clip calculation and upload that value:

```cpp
// REMOVE
const float farClip = glm::min(m_CameraData.FarClip, glm::max(s_ShadowDistance, nearClip));
//

// ADD HERE
const float farClip = glm::max(m_CameraData.FarClip, nearClip);
m_ShadowDepthData.ShadowDistance = farClip;
//
```

## 8. Populate per-cascade scale and transition values

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Location: `FillShadowData`, inside the cascade loop, immediately after `worldUnitsPerTexel` is calculated.

```cpp
const float worldUnitsPerTexel =
    (2.0f * radius) / static_cast<float>(s_ShadowMapSize);

// ADD HERE
const float previousSplit = cascade == 0 ? nearClip : splits[cascade - 1];
m_ShadowDepthData.Cascades[cascade].WorldUnitsPerTexel = worldUnitsPerTexel;
m_ShadowDepthData.Cascades[cascade].TransitionStart =
    glm::mix(previousSplit, cascadeFar, 0.9f);
//
```

## 9. Rename the shadow-bias fields and express both values in texels

File: `EppoEngine/Source/Renderer/SceneRenderer.h`

Rename both members so their units are part of the API:

```cpp
struct ShadowDepthData
{
    std::array<Cascade, s_ShadowCascadeCount> Cascades;
    uint32_t ShadowMapIndex;
    uint32_t ShadowSamplerIndex;

    // REMOVE
    float DepthBias;
    float NormalBias;
    //

    // ADD HERE
    float DepthBiasTexels;
    float NormalBiasTexels;
    //

    float InvMapSize;
    float ShadowDistance;
};
```

Files:

- `EppoEditor/Resources/Shaders/lighting.hlsl`
- `EppoEditor/Resources/Shaders/shadowDepth.hlsl`

Apply the same member rename to both HLSL declarations:

```hlsl
struct ShadowDepthData
{
    Cascade Cascades[s_CascadeCount];
    uint ShadowMapIndex;
    uint ShadowSamplerIndex;

    // REMOVE
    float DepthBias;
    float NormalBias;
    //

    // ADD HERE
    float DepthBiasTexels;
    float NormalBiasTexels;
    //

    float InvMapSize;
    float ShadowDistance;
};
```

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

```cpp
// REMOVE
constexpr float s_ShadowBias = 0.0015f;
constexpr float s_ShadowNormalBias = 0.002f;
//

// ADD HERE
constexpr float s_ShadowDepthBiasTexels = 2.0f;
constexpr float s_ShadowNormalBiasTexels = 1.0f;
//
```

Change the `ShadowDepthData` initializer to use the renamed members and constants:

```cpp
m_ShadowDepthData = {
    // REMOVE
    .DepthBias = s_ShadowBias,
    .NormalBias = s_ShadowNormalBias,
    //

    // ADD HERE
    .DepthBiasTexels = s_ShadowDepthBiasTexels,
    .NormalBiasTexels = s_ShadowNormalBiasTexels,
    //

    .InvMapSize = 1.0f / s_ShadowMapSize,
};
```

## 10. Split the directional shadow calculation into validation, sampling, and blending

File: `EppoEditor/Resources/Shaders/lighting.hlsl`

Remove `CalcShadowFactor`, then add the following functions at the same location.

```hlsl
// REMOVE
// The complete float CalcShadowFactor(const float3 worldPosition, const float3 worldNormal)
// function, from its declaration through its closing brace.
//

// ADD HERE
bool SampleDirectionalCascade(
    const uint cascade,
    const float3 worldPosition,
    const float3 worldNormal,
    const float3 L,
    out float visibility
)
{
    const Cascade cascadeData = uShadowDepth.Cascades[cascade];
    const float3 N = normalize(worldNormal);
    const float slope = 1.0 - saturate(dot(N, L));
    const float normalOffset =
        cascadeData.WorldUnitsPerTexel * uShadowDepth.NormalBiasTexels * slope;

    const float4 lightClip = mul(
        cascadeData.LightViewProjection,
        float4(worldPosition + N * normalOffset, 1.0)
    );

    if (lightClip.w <= 0.0)
        return false;

    const float3 lightNdc = lightClip.xyz / lightClip.w;
    if (lightNdc.z < 0.0 || lightNdc.z > 1.0)
        return false;

    const float2 uv = lightNdc.xy * float2(0.5, -0.5) + 0.5;
    const float border = uShadowDepth.InvMapSize * 1.5;
    if (any(uv < border) || any(uv > 1.0 - border))
        return false;

    Texture2DArray<float> shadowMap =
        ResourceDescriptorHeap[uShadowDepth.ShadowMapIndex];
    SamplerState shadowSampler =
        SamplerDescriptorHeap[uShadowDepth.ShadowSamplerIndex];

    const float compareDepth =
        lightNdc.z - uShadowDepth.DepthBiasTexels * uShadowDepth.InvMapSize;

    float visibleSamples = 0.0;
    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            const float3 sampleUv = float3(
                uv + float2(x, y) * uShadowDepth.InvMapSize,
                cascade
            );

            const float storedDepth =
                shadowMap.SampleLevel(shadowSampler, sampleUv, 0);
            visibleSamples += compareDepth <= storedDepth ? 1.0 : 0.0;
        }
    }

    visibility = visibleSamples / 9.0;
    return true;
}

float CalcShadowFactor(
    const float3 worldPosition,
    const float3 worldNormal,
    const float3 L
)
{
    const float viewDepth =
        -mul(uCamera.View, float4(worldPosition, 1.0)).z;

    if (viewDepth <= 0.0 || viewDepth > uShadowDepth.ShadowDistance)
        return 1.0;

    uint cascade = 0;
    while (
        cascade + 1 < s_CascadeCount &&
        viewDepth > uShadowDepth.Cascades[cascade].SplitDistance
    )
    {
        cascade++;
    }

    float visibility = 1.0;
    bool valid = SampleDirectionalCascade(
        cascade,
        worldPosition,
        worldNormal,
        L,
        visibility
    );

    while (!valid && cascade + 1 < s_CascadeCount)
    {
        cascade++;
        valid = SampleDirectionalCascade(
            cascade,
            worldPosition,
            worldNormal,
            L,
            visibility
        );
    }

    if (!valid)
        return 1.0;

    if (cascade + 1 < s_CascadeCount)
    {
        const Cascade cascadeData = uShadowDepth.Cascades[cascade];
        if (viewDepth >= cascadeData.TransitionStart)
        {
            float nextVisibility = 1.0;
            if (SampleDirectionalCascade(
                cascade + 1,
                worldPosition,
                worldNormal,
                L,
                nextVisibility
            ))
            {
                const float blend = saturate(
                    (viewDepth - cascadeData.TransitionStart) /
                    max(
                        cascadeData.SplitDistance - cascadeData.TransitionStart,
                        0.0001
                    )
                );
                visibility = lerp(visibility, nextVisibility, blend);
            }
        }
    }
    else
    {
        const float fadeStart =
            uShadowDepth.Cascades[s_CascadeCount - 1].TransitionStart;
        const float fade = saturate(
            (uShadowDepth.ShadowDistance - viewDepth) /
            max(uShadowDepth.ShadowDistance - fadeStart, 0.0001)
        );
        visibility = lerp(1.0, visibility, fade);
    }

    return visibility;
}
//
```

Change the directional-light call:

```hlsl
// REMOVE
Lo += CalcShadowFactor(worldPosition, N) *
    BRDF(albedo, L, V, N, metallic, roughness, radiance);
//

// ADD HERE
Lo += CalcShadowFactor(worldPosition, N, L) *
    BRDF(albedo, L, V, N, metallic, roughness, radiance);
//
```

## 11. Add a far-cascade renderer regression test

File: `EppoEngineTesting/Source/Renderer/SceneRendering.cpp`

```cpp
// ADD AFTER SceneRenderer_DirectionalShadowDarkensReceiverAndSurvivesResize
TEST(Renderer, SceneRenderer_DirectionalShadowRemainsValidAcrossAllCascades)
{
    // Add receiver/caster pairs at approximately 2, 7, 18, and 45 world units.
    // Require distinguishable lit and shadowed samples in every range.
    // Move the camera slightly across a split and render again.
    // Require stationary world samples to retain the same visibility when cascade windows move.
}
//
```

## 12. Verify Stage 1

1. Compile every shader.
2. Build the editor, engine, and test targets.
3. Run the mesh and scene-rendering tests added in Steps 5 and 11.
4. Render masked geometry and compare its visible silhouette with its directional-shadow silhouette.
5. Render the front and back of a double-sided material and compare both shadow silhouettes.
6. Place receiver/caster pairs in all four cascade ranges and move the camera across every split.
7. Set the camera far clip above 100 units and confirm the last cascade covers the requested distance.
8. Launch the editor with validation layers and capture the four cascade layers in RenderDoc.

# Stage 2 — Point-light range and shadows

## 13. Add `Range` to `PointLightComponent`

File: `EppoEngine/Source/Scene/Components.h`

```cpp
struct PointLightComponent
{
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 10.0f;

    // ADD HERE
    float Range = 10.0f;
    //
};
```

## 14. Serialize `Range` with a default for older scenes

File: `EppoEngine/Source/Scene/SceneSerializer.cpp`

Inside the point-light deserialization block:

```cpp
nc.Color = c["Color"].get<glm::vec3>();
nc.Intensity = c["Intensity"].get<float>();

// ADD HERE
if (c.contains("Range"))
    nc.Range = c["Range"].get<float>();
//
```

Inside the point-light serialization block:

```cpp
e["PointLightComponent"]["Color"] = c.Color;
e["PointLightComponent"]["Intensity"] = c.Intensity;

// ADD HERE
e["PointLightComponent"]["Range"] = c.Range;
//
```

## 15. Add the editor control

File: `EppoEditor/Source/Panels/PropertyPanel.cpp`

Inside the `DrawComponent<PointLightComponent>` lambda:

```cpp
if (component.Intensity < 0.0f)
    component.Intensity = 0.0f;

// ADD HERE
ImGui::DragFloat("Range", &component.Range, 0.1f, 0.01f, 0.0f);
if (component.Range < 0.01f)
    component.Range = 0.01f;
//
```

## 16. Add the native scripting accessors and registrations

File: `EppoEngine/Source/Scripting/ScriptGlue.cpp`

Add after the point-light intensity setter:

```cpp
// ADD HERE
auto PointLightComponent_GetRange(const uint64_t id) -> float
{
    const Entity entity = GetEntity(id);
    if (!entity || !entity.HasComponent<PointLightComponent>())
        return 0.0f;

    return entity.GetComponent<PointLightComponent>().Range;
}

auto PointLightComponent_SetRange(const uint64_t id, const float range) -> void
{
    const Entity entity = GetEntity(id);
    if (!entity || !entity.HasComponent<PointLightComponent>())
        return;

    entity.GetComponent<PointLightComponent>().Range = glm::max(range, 0.01f);
}
//
```

Add next to the point-light registration entries:

```cpp
// ADD HERE
{ "PointLightComponent_GetRange", reinterpret_cast<void*>(&PointLightComponent_GetRange) },
{ "PointLightComponent_SetRange", reinterpret_cast<void*>(&PointLightComponent_SetRange) },
//
```

## 17. Add the managed scripting calls and property

File: `EppoScriptCore/Source/Core/InternalCalls.cs`

Add after the point-light intensity calls:

```csharp
// ADD HERE
internal static float PointLightComponent_GetRange(ulong id)
{
    return ((delegate* unmanaged[Cdecl]<ulong, float>)Get("PointLightComponent_GetRange"))(id);
}

internal static void PointLightComponent_SetRange(ulong id, float range)
{
    ((delegate* unmanaged[Cdecl]<ulong, float, void>)Get("PointLightComponent_SetRange"))(id, range);
}
//
```

File: `EppoScriptCore/Source/Scene/Components.cs`

Inside the managed `PointLightComponent` class:

```csharp
// ADD HERE
public float Range
{
    get => InternalCalls.PointLightComponent_GetRange(Entity.ID);
    set => InternalCalls.PointLightComponent_SetRange(Entity.ID, value);
}
//
```

## 18. Carry range through point-light submission

File: `EppoEngine/Source/Renderer/SceneRenderer.h`

```cpp
// CHANGE
auto SubmitPointLight(
    const glm::vec3& position,
    const glm::vec3& color,
    float intensity,
    float range
) -> void;
```

File: `EppoEngine/Source/Scene/Scene.cpp`

Inside the point-light submission loop:

```cpp
// CHANGE
sceneRenderer->SubmitPointLight(
    glm::vec3(worldTransform[3]),
    lightComponent.Color,
    lightComponent.Intensity,
    lightComponent.Range
);
```

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Add `range` to the definition signature, then change the submitted position value:

```cpp
// CHANGE
.Position = glm::vec4(position, glm::max(range, 0.01f)),
```

## 19. Apply finite-range attenuation

File: `EppoEditor/Resources/Shaders/lighting.hlsl`

Inside the point-light loop, after `distanceSquared` is calculated:

```hlsl
// ADD HERE
const float range = light.Position.w;
const float distance = sqrt(distanceSquared);
if (distance >= range)
    continue;
//
```

Replace the attenuation line with the finite-range window:

```hlsl
// REMOVE
const float attenuation = 1.0 / distanceSquared;
//

// ADD HERE
const float normalizedDistance = distance / range;
const float squaredDistance = normalizedDistance * normalizedDistance;
const float rangeWindow = saturate(1.0 - squaredDistance * squaredDistance);
const float attenuation = rangeWindow * rangeWindow / distanceSquared;
//
```

## 20. Add the point-shadow render pass and cube array

File: `EppoEngine/Source/Renderer/SceneRenderer.h`

Add next to the directional shadow members:

```cpp
// ADD HERE
Ref<RenderPass> m_PointShadowDepthPass = nullptr;
Ref<Pipeline> m_PointShadowDepthDoubleSidedPipeline = nullptr;
//
```

Add the pass method next to `ShadowDepthPass`:

```cpp
// ADD HERE
auto PointShadowDepthPass() -> void;
//
```

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Add constants next to the directional shadow constants:

```cpp
// ADD HERE
constexpr uint32_t s_PointShadowMapSize = 512;
constexpr float s_PointShadowNearClip = 0.05f;
//
```

Add a constructor block immediately after the directional shadow block:

```cpp
// ADD HERE
// Point Shadow Depth
{
    const auto pointShadowMap = Image::Create(
        ImageSpecification{
            .ImageFormat = nvrhi::Format::D32,
            .Width = s_PointShadowMapSize,
            .Height = s_PointShadowMapSize,
            .ArraySize = MaxPointLights * 6,
            .IsCubemap = true,
            .IsRenderTarget = true,
            .DebugName = "Image Point Shadow Cubes",
        }
    );

    const FramebufferSpecification framebufferSpec{
        .Width = s_PointShadowMapSize,
        .Height = s_PointShadowMapSize,
        .Attachments = { FramebufferTextureSpecification(pointShadowMap) },
        .DebugName = "Framebuffer Point Shadow Cubes",
    };
    const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);
    const nvrhi::TextureSubresourceSet pointShadowSubresources(
        0,
        1,
        0,
        MaxPointLights * 6
    );

    const PipelineSpecification pipelineSpec{
        .Shader = renderer->GetShader("shadowDepth"),
        .CullMode = nvrhi::RasterCullMode::Front,
        .DepthTestEnable = true,
        .DepthWriteEnable = true,
    };

    const RenderPassSpecification renderPassSpec{
        .Name = "Point Shadow Depth",
        .Pipeline = CreateRef<Pipeline>(
            pipelineSpec,
            framebuffer->GetFramebuffer(pointShadowSubresources)->getFramebufferInfo()
        ),
        .Framebuffer = framebuffer,
        .Subresources = pointShadowSubresources,
        .ClearDepthOnLoad = true,
        .DepthClearValue = 1.0f,
    };
    m_PointShadowDepthPass = CreateRef<RenderPass>(renderPassSpec);

    auto doubleSidedPipelineSpec = pipelineSpec;
    doubleSidedPipelineSpec.CullMode = nvrhi::RasterCullMode::None;
    m_PointShadowDepthDoubleSidedPipeline = CreateRef<Pipeline>(
        doubleSidedPipelineSpec,
        framebuffer->GetFramebuffer(pointShadowSubresources)->getFramebufferInfo()
    );
}
//
```

## 21. Extend the shadow data for point faces

File: `EppoEngine/Source/Renderer/SceneRenderer.h`

Add after the directional cascade count:

```cpp
// ADD HERE
static constexpr uint32_t s_PointShadowFaceCount = MaxPointLights * 6;
//
```

Append to `ShadowDepthData` after `ShadowDistance`:

```cpp
// ADD HERE
std::array<glm::mat4, s_PointShadowFaceCount> PointLightViewProjections;
uint32_t PointShadowMapIndex;
uint32_t PointShadowSamplerIndex;
uint32_t PointShadowLightCount;
uint32_t PointShadowPadding;
//
```

Append matching fields to `ShadowDepthData` in `lighting.hlsl` and `shadowDepth.hlsl` after `ShadowDistance`:

```hlsl
// ADD HERE
float4x4 PointLightViewProjections[s_PointShadowFaceCount];
uint PointShadowMapIndex;
uint PointShadowSamplerIndex;
uint PointShadowLightCount;
uint PointShadowPadding;
//
```

## 22. Extend `shadowDepth.hlsl` with point-shadow mode

Add shadow-mode fields to the push constants:

```hlsl
struct PushConstants
{
    uint DrawIndex;

    // ADD HERE
    uint ShadowType; // 0 directional, 1 point
    uint ProjectionCount;
    uint Padding;
    //
};
```

Add world position to the varyings so point mode can write radial depth:

```hlsl
struct Varyings
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;

    // ADD HERE
    float3 WorldPosition : POSITION0;
    //

    uint Layer : SV_RenderTargetArrayIndex;
};
```

Wrap the directional projection calculation in `ShadowType == 0`, then add a point branch that calculates `pointFace = input.InstanceID % ProjectionCount`, calculates the object instance by division, transforms with `PointLightViewProjections[pointFace]`, assigns `WorldPosition`, and assigns `Layer = pointFace`.

Change `PSMain` to return `SV_Depth`. Execute the masked-alpha `clip`, return `input.Position.z` when `ShadowType == 0`, and otherwise return the distance from `WorldPosition` to `uLights.Lights[input.Layer / 6].Position.xyz` divided by `uLights.Lights[input.Layer / 6].Position.w`.

## 23. Record the point-shadow pass

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Add `PointShadowDepthPass` immediately after `ShadowDepthPass`. Its primitive traversal is the same DrawIndex traversal as directional shadows, except:

```cpp
// ADD TO ITS PC INITIALIZATION
pushConstants.ShadowType = 1;
pushConstants.ProjectionCount = m_LightData.NumLights * 6;
//
```

The layered instance count is:

```cpp
// ADD HERE
const uint32_t layeredInstanceCount =
    instanceCount * pushConstants.ProjectionCount;
//
```

Add an early return after recording an empty timer query when `NumLights == 0`. Add the blend check, point-shadow pipeline selection, and `drawIndex == m_DrawData.size()` assertion to the point-shadow traversal.

Bind these resources to the point-shadow pass:

```cpp
// ADD HERE
m_PointShadowDepthPass->SetInput(0, 0, m_InstanceTransformsSB);
m_PointShadowDepthPass->SetInput(0, 1, m_ShadowDepthUB);
m_PointShadowDepthPass->SetInput(0, 1, m_DrawDataSB);
m_PointShadowDepthPass->SetInput(0, 2, m_MaterialDataSB);
m_PointShadowDepthPass->SetInput(0, 3, m_LightsUB);
//
```

Bind `m_LightsUB` at binding 3 on `m_ShadowDepthPass`:

```cpp
// ADD HERE
m_ShadowDepthPass->SetInput(0, 3, m_LightsUB);
//
```

## 24. Sample point shadows during lighting

File: `EppoEditor/Resources/Shaders/lighting.hlsl`

Add `CalcPointShadowFactor(lightIndex, worldPosition, worldNormal, L)` immediately after the directional shadow functions. It samples `TextureCubeArray<float>` using `float4(fragmentToLightDirection, lightIndex)`, compares `distance / range` with an eight-tap fixed cubemap PCF kernel, and uses a range-relative normal/slope bias.

Change the point-light BRDF accumulation:

```hlsl
// REMOVE
Lo += BRDF(albedo, L, V, N, metallic, roughness, radiance);
//

// ADD HERE
Lo += CalcPointShadowFactor(i, worldPosition, N, L) *
    BRDF(albedo, L, V, N, metallic, roughness, radiance);
//
```

## 25. Account for the point-shadow pass everywhere

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Add `m_PointShadowDepthPass` to:

- The pass tree immediately after `m_ShadowDepthPass`.
- `EndScene` immediately after `ShadowDepthPass()`.
- Per-frame statistics reset.
- Upload-time shadow-map bindless registration.
- Resource-state transition to `ShaderResource` before lighting.
- `Bake()` calls after inputs are assigned.

## 26. Add point-light range and shadow tests

Files:

- `EppoEngineTesting/Source/Scene/Scene.cpp`
- `EppoEngineTesting/Source/Scripting/Scripting.cpp`
- `EppoEngineTesting/Source/Renderer/SceneRendering.cpp`

Add tests for:

- Range serialization with backward-compatible default.
- Range duplication and scene copy.
- Native/managed get and set.
- Finite range suppressing light outside the radius.
- A point-light caster darkening a receiver on multiple cube faces.
- Masked and double-sided point-shadow casters.
- Sixteen point lights rendering with zero validation errors.

## 27. Verify Stage 2

1. Compile `shadowDepth.hlsl` and `lighting.hlsl`.
2. Build the editor, engine, script core, and test targets.
3. Run the scene, scripting, and scene-rendering tests added in Step 26.
4. Render one point light with casters on all six cube faces and inspect every array layer in RenderDoc.
5. Render 16 point lights and inspect the point-shadow pass, lighting bindings, and validation output.
6. Move a receiver across a point light's range and confirm its contribution reaches zero at the configured radius.

# Stage 3 — SSR and basic transparency

## 28. Move indirect specular IBL out of `lighting.hlsl`

File: `EppoEditor/Resources/Shaders/lighting.hlsl`

Remove the reflection vector, prefiltered environment lookup, BRDF LUT lookup, and `specular` calculation, then change the ambient assignment:

```hlsl
// REMOVE FROM THE BAKED-IBL BRANCH
const float3 R = reflect(-V, N);
const float maxLod = 4.0;
const float3 prefiltered =
    prefilterMap.SampleLevel(iblSampler, R, roughness * maxLod).rgb;
const float2 brdf =
    brdfLut.SampleLevel(iblSampler, float2(dotNV, roughness), 0).rg;
const float3 specular = prefiltered * (F0 * brdf.x + brdf.y);
//

// REMOVE
ambient = (kD * diffuse + specular) * uEnvironment.Params.x;
//

// ADD HERE
ambient = kD * diffuse * uEnvironment.Params.x;
//
```

## 29. Add the SSR pass members

File: `EppoEngine/Source/Renderer/SceneRenderer.h`

Add after the sky pass member:

```cpp
// ADD HERE
Ref<RenderPass> m_SsrPass = nullptr;
//
```

Add the method after `SkyPass`:

```cpp
// ADD HERE
auto SsrPass() const -> void;
//
```

## 30. Add `ssr.hlsl`

New file: `EppoEditor/Resources/Shaders/ssr.hlsl`

1. Add `Includes/fullscreen.hlsli` and `Includes/lighting.hlsli`.
2. Add the `Camera` constant buffer at `b2` with the member order used by `lighting.hlsl`.
3. Add the `Environment` constant buffer at `b4` with the member order used by `lighting.hlsl`.
4. Add these texture and sampler declarations:

```hlsl
// ADD HERE
Texture2D uLighting : register(t0, space0);
Texture2D uBaseColorMetalness : register(t1, space0);
Texture2D uNormalRoughness : register(t2, space0);
Texture2D uEmissionAO : register(t3, space0);
Texture2D uDepth : register(t4, space0);
Texture2D uSsao : register(t5, space0);
SamplerState uSampler : register(s0, space0);
//
```

5. Add the fullscreen `VSMain` using `BuildFullscreenTriangleVertex`.
6. Add zero-to-one depth reconstruction with the Y flip used by `lighting.hlsl` and `ssao.hlsl`.
7. Add a 48-step view-space ray march with five binary-refinement steps, a 50-unit maximum distance, roughness-scaled thickness, and edge/distance/facing/roughness confidence fades.
8. Add the prefiltered-environment and BRDF-LUT calculation as the miss result.
9. Return `uLighting` when the sampled depth is background depth.
10. Return `uLighting + indirectSpecular * materialAO * ssaoFactor` for geometry pixels, blending the prefiltered IBL result and screen-space hit by SSR confidence.

## 31. Construct and bind the SSR pass

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Add a constructor block immediately after the sky block:

```cpp
// ADD HERE
// SSR
{
    const FramebufferSpecification framebufferSpec{
        .Width = m_Width,
        .Height = m_Height,
        .Attachments = { nvrhi::Format::RGBA16_FLOAT },
        .DebugName = "Framebuffer SSR",
    };
    const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

    const PipelineSpecification pipelineSpec{
        .Shader = renderer->GetShader("ssr"),
        .CullMode = nvrhi::RasterCullMode::None,
    };

    const RenderPassSpecification renderPassSpec{
        .Name = "SSR",
        .Pipeline = CreateRef<Pipeline>(
            pipelineSpec,
            framebuffer->GetFramebuffer()->getFramebufferInfo()
        ),
        .Framebuffer = framebuffer,
        .ClearColorOnLoad = true,
        .ClearColor = glm::vec4(0.0f),
    };
    m_SsrPass = CreateRef<RenderPass>(renderPassSpec);
}
//
```

Add the SSR inputs after constructing the pass:

```cpp
// ADD HERE
m_SsrPass->SetInput(0, 0, m_LightingPass->GetFramebuffer()->GetFinalImage());
m_SsrPass->SetInput(0, 1, m_GeometryPass->GetFramebuffer()->GetImage(0));
m_SsrPass->SetInput(0, 2, m_GeometryPass->GetFramebuffer()->GetImage(1));
m_SsrPass->SetInput(0, 3, m_GeometryPass->GetFramebuffer()->GetImage(2));
m_SsrPass->SetInput(0, 4, m_GeometryPass->GetFramebuffer()->GetDepthImage());
m_SsrPass->SetInput(0, 5, m_SsaoBlurHorizontalPass->GetFramebuffer()->GetFinalImage());
m_SsrPass->SetInput(0, 0, fullscreenSampler);
m_SsrPass->SetInput(0, 2, m_CameraUB);
m_SsrPass->SetInput(0, 4, m_EnvironmentUB);
//
```

## 32. Record SSR

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Add `SsrPass` after `SkyPass`. It follows the same full-screen triangle structure as `LightingPass`: transition its inputs to `ShaderResource`, begin timer/marker/pass, draw three vertices, update statistics, and end the pass/timer/marker.

Add SSR to:

- Pass tree after sky.
- `EndScene` after `SkyPass()`.
- Statistics reset.
- `Resize` using `m_SsrPass->Resize(m_Width, m_Height)`.
- Resize-time input restoration.
- `Bake()` calls.

## 33. Add transparent draw records

File: `EppoEngine/Source/Renderer/SceneRenderer.h`

Add after `DrawData` storage:

```cpp
// ADD HERE
struct TransparentDraw
{
    const Submesh* Submesh = nullptr;
    const Primitive* Primitive = nullptr;
    uint32_t DrawIndex = 0;
    uint32_t InstanceIndex = 0;
    float DistanceSquared = 0.0f;
};
std::vector<TransparentDraw> m_TransparentDraws;
//
```

## 34. Gather and sort transparent primitive instances

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Location: `PrepareRenderData`, during the primitive traversal that builds `m_DrawData` and `m_MaterialData`.

Clear the vector with the other per-frame vectors:

```cpp
// ADD HERE
m_TransparentDraws.clear();
//
```

After appending a primitive's `DrawData` and `MaterialData`, add one record per instance when `material->AlphaMode == MaterialAlphaMode::Blend`. Assign its draw index and `drawCmd.InstanceOffset + localInstance`, then calculate `DistanceSquared` from `m_CameraData.Position` to the transformed submesh/bounds centre.

After the traversal:

```cpp
// ADD HERE
std::ranges::sort(
    m_TransparentDraws,
    std::greater{},
    &TransparentDraw::DistanceSquared
);
//
```

## 35. Add `transparency.hlsl`

New file: `EppoEditor/Resources/Shaders/transparency.hlsl`

Add the mesh input, camera, instance-transform, `DrawData`, `MaterialData`, directional-shadow, point-shadow, light, and environment declarations used by the geometry and lighting shaders. Add these push constants:

```hlsl
struct PushConstants
{
    uint DrawIndex;
    uint InstanceIndex;
};
```

Fetch `uInstanceTransforms[InstanceIndex]` in the vertex shader so each sorted transparent instance uses the transform named by its draw record.

Implement the pixel shader with these actions:

- Sample the base-colour, metallic/roughness, normal, material AO, and emission inputs using the declarations copied from `geometry.hlsl`.
- Reverse the final normal for a double-sided back face.
- Evaluate the directional light, directional cascades, point lights, point shadows, diffuse IBL, and specular IBL fallback with the functions copied from `lighting.hlsl`.
- Multiply ambient lighting by material AO.
- Return the resulting HDR colour with `saturate(baseColor.a)`.

```hlsl
// FINAL TRANSPARENT OUTPUT
return float4(finalLighting, saturate(baseColor.a));
```

That returned alpha is consumed by the framebuffer blend state below.

## 36. Add the transparency pass and pipelines

File: `EppoEngine/Source/Renderer/SceneRenderer.h`

```cpp
// ADD HERE
Ref<RenderPass> m_TransparencyPass = nullptr;
Ref<Pipeline> m_TransparencyDoubleSidedPipeline = nullptr;
//
```

Add the method next to SSR:

```cpp
// ADD HERE
auto TransparencyPass() -> void;
//
```

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Add after the SSR constructor block. Its framebuffer uses the SSR output colour and geometry depth:

```cpp
// ADD HERE
const auto transparencyFramebuffer = CreateRef<Framebuffer>(
    FramebufferSpecification{
        .Width = m_Width,
        .Height = m_Height,
        .Attachments = {
            FramebufferTextureSpecification(
                m_SsrPass->GetFramebuffer()->GetFinalImage()
            ),
            FramebufferTextureSpecification(
                m_GeometryPass->GetFramebuffer()->GetDepthImage()
            ),
        },
        .DebugName = "Framebuffer Transparency",
    }
);

nvrhi::BlendState blendState{};
auto& target = blendState.targets[0];
target.blendEnable = true;
target.srcBlend = nvrhi::BlendFactor::SrcAlpha;
target.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
target.blendOp = nvrhi::BlendOp::Add;
target.srcBlendAlpha = nvrhi::BlendFactor::One;
target.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
target.blendOpAlpha = nvrhi::BlendOp::Add;

const PipelineSpecification pipelineSpec{
    .Shader = renderer->GetShader("transparency"),
    .CullMode = nvrhi::RasterCullMode::Front,
    .DepthTestEnable = true,
    .DepthWriteEnable = false,
    .BlendState = blendState,
};

const RenderPassSpecification renderPassSpec{
    .Name = "Transparency",
    .Pipeline = CreateRef<Pipeline>(
        pipelineSpec,
        transparencyFramebuffer->GetFramebuffer()->getFramebufferInfo()
    ),
    .Framebuffer = transparencyFramebuffer,
    .OwnsFramebuffer = false,
};
m_TransparencyPass = CreateRef<RenderPass>(renderPassSpec);

auto doubleSidedPipelineSpec = pipelineSpec;
doubleSidedPipelineSpec.CullMode = nvrhi::RasterCullMode::None;
m_TransparencyDoubleSidedPipeline = CreateRef<Pipeline>(
    doubleSidedPipelineSpec,
    transparencyFramebuffer->GetFramebuffer()->getFramebufferInfo()
);
//
```

## 37. Record sorted transparency

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Add `TransparencyPass` after `SsrPass`. Begin the non-clearing transparency pass, iterate `m_TransparentDraws` in its sorted order, bind each referenced submesh buffers, select the normal or double-sided transparency pipeline, push `DrawIndex` and `InstanceIndex`, and issue an indexed draw with `instanceCount = 1`.

## 38. Rebuild the non-owning transparency framebuffer on resize

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Location: the resize submission after the owned passes resize and their image handles change.

Add a framebuffer using the resized SSR colour and geometry depth, then assign it to the transparency pass:

```cpp
// ADD HERE
const auto transparencyFramebuffer = CreateRef<Framebuffer>(
    FramebufferSpecification{
        .Width = m_Width,
        .Height = m_Height,
        .Attachments = {
            FramebufferTextureSpecification(
                m_SsrPass->GetFramebuffer()->GetFinalImage()
            ),
            FramebufferTextureSpecification(
                m_GeometryPass->GetFramebuffer()->GetDepthImage()
            ),
        },
        .DebugName = "Framebuffer Transparency",
    }
);
m_TransparencyPass->SetFramebuffer(transparencyFramebuffer);
//
```

Set every SSR and transparency input again, then call `Bake()` on both passes after the resize-time binding updates.

## 39. Change bloom to consume the post-SSR/post-transparency HDR image

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Inside `BloomPass`, change the first source image used for mip-zero downsampling:

```cpp
// REMOVE
const auto& source = m_LightingPass->GetFramebuffer()->GetFinalImage();
//

// ADD HERE
const auto& source = m_SsrPass->GetFramebuffer()->GetFinalImage();
//
```

Change the tonemap HDR input from the lighting image to the SSR image at its constructor binding and resize-time rebinding.

## 40. Account for SSR and transparency everywhere

File: `EppoEngine/Source/Renderer/SceneRenderer.cpp`

Add both passes to:

- Pass tree after sky and before bloom.
- `EndScene` as `SsrPass(); TransparencyPass();` after `SkyPass()`.
- Statistics reset.
- Resize/rebinding and `Bake()`.
- Resource-state transitions.

Add `m_SsrPass->Resize(m_Width, m_Height)` to viewport resizing. Rebuild the transparency framebuffer with the delta in Step 38.

## 41. Add SSR and transparency tests

File: `EppoEngineTesting/Source/Renderer/SceneRendering.cpp`

Add tests for:

- A smooth reflective plane receiving an on-screen reflection.
- Roughness suppressing SSR confidence and using prefiltered IBL fallback.
- An SSR miss retaining the prefiltered specular IBL result.
- SSR surviving viewport resize and binding recreation.
- A blended material compositing over opaque HDR lighting.
- Back-to-front ordering of two blended instances.
- Transparent depth testing with depth writes disabled.
- Double-sided transparent rendering.
- Transparent objects receiving directional and point shadows.
- Bloom and tonemap consuming the post-transparency HDR result.

## 42. Verify Stage 3

1. Compile `lighting.hlsl`, `ssr.hlsl`, and `transparency.hlsl`.
2. Build the editor, engine, and test targets.
3. Run the scene-rendering tests added in Step 41, then run the full test suite.
4. Capture a reflective opaque surface and inspect the SSR hit, IBL fallback, and combined HDR output in RenderDoc.
5. Capture two overlapping transparent instances and inspect their draw order, blend state, depth test, and disabled depth writes.
6. Resize the viewport and capture SSR, transparency, bloom, and tonemap bindings again.
7. Launch the editor with validation layers and verify the pass order: directional shadow, point shadows, geometry, SSAO, lighting, sky, SSR, transparency, bloom downsample, bloom upsample, tonemap, and wireframe.
