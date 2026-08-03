#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include "Platform/Vulkan/DeviceManagerVK.h"
#include "Renderer/DeviceManager.h"

using namespace Eppo;

TEST(Renderer, DeviceManager_UnderHarness_ExposesLiveDevice)
{
	if (!Testing::AppHarness::IsAvailable())
		return;

	const auto& dm = DeviceManager::Get();
	EP_REQUIRE(dm);
	EXPECT_TRUE(dm->GetDevice());
	EXPECT_TRUE(dm->GetRenderer());
}

TEST(Renderer, PhysicalDevice_ReportsAllRequiredFeatures)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto& dm = DeviceManager::Get();
    const auto* deviceManager = dynamic_cast<DeviceManagerVK*>(dm.get());
    EP_REQUIRE(deviceManager);
    EP_REQUIRE(deviceManager->GetPhysicalDevice());
    EXPECT_TRUE(deviceManager->GetPhysicalDevice()->SupportsRequiredFeatures());
}
