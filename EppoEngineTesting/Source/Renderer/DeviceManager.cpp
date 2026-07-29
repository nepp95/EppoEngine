#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

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
}
