#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include "Renderer/Framebuffer.h"

using namespace Eppo;

SUITE(Renderer)
{
    TEST(Framebuffer_CreatedWithExplicitSize_HasMatchingDimensionsAndHandle)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const FramebufferSpecification spec{
            .Width = 256,
            .Height = 256,
            .Attachments = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::D32 },
            .DebugName = "Framebuffer RendererTest",
        };

        const auto framebuffer = CreateRef<Framebuffer>(spec);

        CHECK(framebuffer->GetFramebuffer());
        CHECK_EQUAL(256, framebuffer->GetWidth());
        CHECK_EQUAL(256, framebuffer->GetHeight());
    }
}
