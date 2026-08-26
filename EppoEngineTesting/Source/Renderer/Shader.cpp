#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/Vertex.h"

using namespace Eppo;

namespace
{
	class ShaderWithoutStaticBindings final : public Shader
	{
	public:
		ShaderWithoutStaticBindings()
			: Shader({ .Name = "ShaderWithoutStaticBindings" })
		{
		}

		auto CreateLayouts() -> void { CreateBindingLayout(); }
	};

	// Compiles only if the named include is resolved, so a successful build proves where it came from.
	auto SourceRequiring(const std::string& include) -> std::string
	{
		return std::format("#include \"{}\"\nfloat4 VSMain(float3 inPosition : POSITION) : SV_Position\n{{\n\treturn PackedValue(inPosition);\n}}\n", include);
	}

	// A packed shader must compile fresh: a cache hit from an earlier run would bypass include resolution entirely.
	auto DiscardShaderCache(const std::string& name) -> void
	{
		FS::RemoveAll(FS::GetShaderCacheDirectory() / std::format("{}.vert.spv", name));
		FS::RemoveAll(FS::GetShaderCacheDirectory() / std::format("{}.hash", name));
	}

    auto CheckMeshVertexLayout(const Ref<Shader>& shader) -> void
    {
        EP_REQUIRE(shader != nullptr);

        const auto& inputLayout = shader->GetInputLayout();
        EP_REQUIRE(inputLayout != nullptr);
        EXPECT_EQ(4u, inputLayout->getNumAttributes());

        bool foundTangent = false;
        for (uint32_t i = 0; i < inputLayout->getNumAttributes(); i++)
        {
            const auto* attribute = inputLayout->getAttributeDesc(i);
            EP_REQUIRE(attribute != nullptr);
            EXPECT_EQ(static_cast<uint32_t>(sizeof(Vertex)), attribute->elementStride);

            if (attribute->name.ends_with("TANGENT0"))
            {
                foundTangent = true;
                EXPECT_TRUE(attribute->format == nvrhi::Format::RGBA32_FLOAT);
                EXPECT_EQ(static_cast<uint32_t>(offsetof(Vertex, Tangent)), attribute->offset);
            }
        }

        EXPECT_TRUE(foundTangent);
    }

    [[nodiscard]] auto HasResource(
        const Ref<Shader>& shader, const uint32_t set, const uint32_t binding, const nvrhi::ResourceType type
    ) -> bool
    {
        const auto& resources = shader->GetShaderResources();
        if (!resources.contains(set))
            return false;

        return std::ranges::any_of(
            resources.at(set),
            [binding, type](const ShaderResourceBinding& resource) -> bool
            {
                return resource.Binding == binding && resource.Type == type;
            }
        );
    }
}

TEST(Renderer, Shader_PackedSourcesResolveIncludesFromThePack)
{
	if (!Testing::AppHarness::IsAvailable())
		return;

	const std::string name = "PackedIncludeServedFromPack";
	DiscardShaderCache(name);
	const auto include = std::string("Includes/only-in-pack.hlsli");
	EP_REQUIRE(!FS::Exists(FS::GetResourcesDirectory() / "Shaders" / include));

	const Ref<Shader> shader = Shader::Create(ShaderSpecification{
		.Name = name,
		.Source = SourceRequiring(include),
		.Includes = { { include, "float4 PackedValue(float3 position) { return float4(position, 1.0); }" } },
	});

	EP_REQUIRE(shader != nullptr);
	EXPECT_TRUE(shader->GetShaderHandle(nvrhi::ShaderType::Vertex) != nullptr);
}

TEST(Renderer, Shader_PackedIncludesShadowTheSameFileOnDisk)
{
	if (!Testing::AppHarness::IsAvailable())
		return;

	const std::string name = "PackedIncludeShadowsDisk";
	DiscardShaderCache(name);
	// This one does sit on disk beside the executable, but it declares only PUSH_CONSTANTS. PackedValue
	// exists solely in the packed copy, so compiling proves the disk file was not the one that resolved.
	const auto include = std::string("Includes/platform.hlsli");
	EP_REQUIRE(FS::Exists(FS::GetResourcesDirectory() / "Shaders" / include));

	const Ref<Shader> shader = Shader::Create(ShaderSpecification{
		.Name = name,
		.Source = SourceRequiring(include),
		.Includes = { { include, "float4 PackedValue(float3 position) { return float4(position, 1.0); }" } },
	});

	EP_REQUIRE(shader != nullptr);
	EXPECT_TRUE(shader->GetShaderHandle(nvrhi::ShaderType::Vertex) != nullptr);
}

TEST(Renderer, Shader_GeometryUsesSharedBindlessHeapLayouts)
{
	if (!Testing::AppHarness::IsAvailable())
		return;

	const auto& renderer = DeviceManager::Get()->GetRenderer();
	const auto& descriptorManager = renderer->GetDescriptorManager();
	const auto& layouts = renderer->GetShader("geometry")->GetBindingLayouts();

	EXPECT_EQ(3u, static_cast<uint32_t>(layouts.size()));
	EXPECT_TRUE(layouts.at(1).Get() == descriptorManager->GetResourceHeap()->BindingLayout.Get());
	EXPECT_TRUE(layouts.at(2).Get() == descriptorManager->GetSamplerHeap()->BindingLayout.Get());
}

TEST(Renderer, Shader_MeshVertexLayoutsIncludeTangentsWithVertexStride)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& renderer = DeviceManager::Get()->GetRenderer();
    CheckMeshVertexLayout(renderer->GetShader("geometry"));
    CheckMeshVertexLayout(renderer->GetShader("wireframe"));
}

TEST(Renderer, Shader_ShadowDepthReflectsMeshLayoutAndBindings)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& renderer = DeviceManager::Get()->GetRenderer();
    const Ref<Shader>& shadowDepth = renderer->GetShader("shadowDepth");
    CheckMeshVertexLayout(shadowDepth);

    EXPECT_TRUE(HasResource(shadowDepth, 0, 0, nvrhi::ResourceType::StructuredBuffer_SRV));
    EXPECT_TRUE(HasResource(shadowDepth, 0, 1, nvrhi::ResourceType::ConstantBuffer));
    EP_REQUIRE(shadowDepth->HasPushConstants());
    EXPECT_EQ(68u, shadowDepth->GetPushConstants().Size);

    EXPECT_TRUE(HasResource(renderer->GetShader("geometry"), 0, 2, nvrhi::ResourceType::ConstantBuffer));
}

TEST(Renderer, Shader_TonemapReflectsTextureSamplerAndExposure)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& renderer = DeviceManager::Get()->GetRenderer();
    const auto& shaders = renderer->GetAllShaders();
    EP_REQUIRE(shaders.contains("tonemap"));

    const auto& shader = shaders.at("tonemap");
    EXPECT_TRUE(HasResource(shader, 0, 0, nvrhi::ResourceType::Texture_SRV));
    EXPECT_TRUE(HasResource(shader, 0, 1, nvrhi::ResourceType::Texture_SRV));
    EXPECT_TRUE(HasResource(shader, 0, 0, nvrhi::ResourceType::Sampler));
    EP_REQUIRE(shader->HasPushConstants());
    EXPECT_EQ(16u, shader->GetPushConstants().Size);
}

TEST(Renderer, Shader_WireframeReflectsSceneDepthTexture)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& shader = DeviceManager::Get()->GetRenderer()->GetShader("wireframe");
    const auto& resources = shader->GetShaderResources();
    EP_REQUIRE(resources.contains(0));

    const auto& setResources = resources.at(0);
    const auto hasSceneDepth = std::ranges::any_of(
        setResources,
        [](const ShaderResourceBinding& resource) -> bool
        {
            return resource.Binding == 2 && resource.Type == nvrhi::ResourceType::Texture_SRV;
        }
    );

    EXPECT_TRUE(hasSceneDepth);
}

TEST(Renderer, Shader_ImGuiUsesSharedBindlessHeapLayoutsAlongsideStaticBindings)
{
	if (!Testing::AppHarness::IsAvailable())
		return;

	const auto& renderer = DeviceManager::Get()->GetRenderer();
	const auto& descriptorManager = renderer->GetDescriptorManager();
	const auto& layouts = renderer->GetShader("imgui")->GetBindingLayouts();

	EXPECT_EQ(3u, static_cast<uint32_t>(layouts.size()));
	EXPECT_TRUE(layouts.contains(0));
	EXPECT_TRUE(layouts.at(1).Get() == descriptorManager->GetResourceHeap()->BindingLayout.Get());
	EXPECT_TRUE(layouts.at(2).Get() == descriptorManager->GetSamplerHeap()->BindingLayout.Get());
}

TEST(Renderer, Shader_WithoutStaticBindingsStillProducesGaplessLayouts)
{
	if (!Testing::AppHarness::IsAvailable())
		return;

	ShaderWithoutStaticBindings shader;
	shader.CreateLayouts();

	const auto& layouts = shader.GetBindingLayouts();
	EXPECT_EQ(3u, static_cast<uint32_t>(layouts.size()));
	EXPECT_TRUE(layouts.contains(0));
	EXPECT_TRUE(layouts.at(0));
	EXPECT_TRUE(layouts.contains(1));
	EXPECT_TRUE(layouts.contains(2));
}
