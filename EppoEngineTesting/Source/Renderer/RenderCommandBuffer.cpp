#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/RenderCommandBuffer.h"

using namespace Eppo;

SUITE(Renderer)
{
	TEST(RenderCommandBuffer_AllocatesTimingDataForEveryBackBuffer)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const RenderCommandBuffer renderCommandBuffer;
		const uint32_t frameIndex = DeviceManager::Get()->GetBackBufferCount() - 1;

		CHECK_EQUAL(0.0f, renderCommandBuffer.GetTime(frameIndex));
		CHECK_EQUAL(0.0f, renderCommandBuffer.GetTimeMs(frameIndex));
		CHECK_EQUAL(0.0f, renderCommandBuffer.GetTime(frameIndex + 1));
		CHECK_EQUAL(0.0f, renderCommandBuffer.GetTime("Unrecorded", frameIndex + 1));
	}

	TEST(RenderCommandBuffer_BeginEndSubmitCompletes)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		RenderCommandBuffer renderCommandBuffer;
		const uint32_t frameIndex = DeviceManager::Get()->GetCurrentBackBufferIndex();

		renderCommandBuffer.Begin("Test");
		CHECK(renderCommandBuffer.GetCommandList());
		renderCommandBuffer.End();
		renderCommandBuffer.Submit();

		CHECK(renderCommandBuffer.GetTime(frameIndex) >= 0.0f);
	}

	TEST(RenderCommandBuffer_NamedTimerIsReadableAfterSubmit)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		RenderCommandBuffer renderCommandBuffer;
		const uint32_t frameIndex = DeviceManager::Get()->GetCurrentBackBufferIndex();

		renderCommandBuffer.Begin();
		renderCommandBuffer.BeginTimerQuery("Pass");
		renderCommandBuffer.EndTimerQuery("Pass");
		renderCommandBuffer.End();
		renderCommandBuffer.Submit();

		CHECK(renderCommandBuffer.GetTime("Pass", frameIndex) >= 0.0f);
	}
}
