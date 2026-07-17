#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RenderPass.h"

#include <nvrhi/nvrhi.h>

using namespace Eppo;

SUITE(Renderer)
{
    TEST(DeviceManager_UnderHarness_ExposesLiveDevice)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto& dm = DeviceManager::Get();
        REQUIRE CHECK(dm);

        // A booted harness must hand us a real nvrhi device and renderer.
        CHECK(dm->GetDevice());
        CHECK(dm->GetRenderer());
    }

    TEST(Framebuffer_CreatedWithExplicitSize_HasMatchingDimensionsAndHandle)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const FramebufferSpecification spec{
            .Width = 256,
            .Height = 256,
            .Attachments = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::D32 },
            .DebugName = "Framebuffer RendererExampleTest",
        };

        const auto framebuffer = CreateRef<Framebuffer>(spec);

        CHECK(framebuffer->GetFramebuffer());
        CHECK_EQUAL(256, framebuffer->GetWidth());
        CHECK_EQUAL(256, framebuffer->GetHeight());
    }
}
