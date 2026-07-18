#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/Sampler.h"

using namespace Eppo;

SUITE(Renderer)
{
	TEST(Sampler_OwnsItsBindlessHandle)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const auto descriptorManager = CreateRef<DescriptorManager>();
		const auto sampler = Sampler::Create(descriptorManager);

		CHECK_EQUAL(0u, sampler->GetBindlessIndex());
		CHECK_EQUAL(1u, descriptorManager->GetSamplerHeap()->NextFreeSlot);
	}
}
