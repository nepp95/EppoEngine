#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include "Platform/Vulkan/DeviceManagerVK.h"
#include "Renderer/DeviceManager.h"

using namespace Eppo;

SUITE(Renderer)
{
	TEST(DeviceManager_UnderHarness_ExposesLiveDevice)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const auto& dm = DeviceManager::Get();
		REQUIRE CHECK(dm);
		CHECK(dm->GetDevice());
		CHECK(dm->GetRenderer());
	}

    TEST(PhysicalDevice_ReportsAllRequiredFeatures)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        const auto& dm = DeviceManager::Get();
        const auto* deviceManager = dynamic_cast<DeviceManagerVK*>(dm.get());
        REQUIRE CHECK(deviceManager);
        REQUIRE CHECK(deviceManager->GetPhysicalDevice());
        CHECK(deviceManager->GetPhysicalDevice()->SupportsRequiredFeatures());
    }
}
