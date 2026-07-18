#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"

using namespace Eppo;

SUITE(Renderer)
{
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
	}

	TEST(Shader_GeometryUsesSharedBindlessHeapLayouts)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const auto& renderer = DeviceManager::Get()->GetRenderer();
		const auto& descriptorManager = renderer->GetDescriptorManager();
		const auto& layouts = renderer->GetShader("geometry")->GetBindingLayouts();

		CHECK_EQUAL(3u, static_cast<uint32_t>(layouts.size()));
		CHECK(layouts.at(1).Get() == descriptorManager->GetResourceHeap()->BindingLayout.Get());
		CHECK(layouts.at(2).Get() == descriptorManager->GetSamplerHeap()->BindingLayout.Get());
	}

	TEST(Shader_ImGuiUsesSharedBindlessHeapLayoutsAlongsideStaticBindings)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const auto& renderer = DeviceManager::Get()->GetRenderer();
		const auto& descriptorManager = renderer->GetDescriptorManager();
		const auto& layouts = renderer->GetShader("imgui")->GetBindingLayouts();

		CHECK_EQUAL(3u, static_cast<uint32_t>(layouts.size()));
		CHECK(layouts.contains(0));
		CHECK(layouts.at(1).Get() == descriptorManager->GetResourceHeap()->BindingLayout.Get());
		CHECK(layouts.at(2).Get() == descriptorManager->GetSamplerHeap()->BindingLayout.Get());
	}

	TEST(Shader_WithoutStaticBindingsStillProducesGaplessLayouts)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		ShaderWithoutStaticBindings shader;
		shader.CreateLayouts();

		const auto& layouts = shader.GetBindingLayouts();
		CHECK_EQUAL(3u, static_cast<uint32_t>(layouts.size()));
		CHECK(layouts.contains(0));
		CHECK(layouts.at(0));
		CHECK(layouts.contains(1));
		CHECK(layouts.contains(2));
	}
}
