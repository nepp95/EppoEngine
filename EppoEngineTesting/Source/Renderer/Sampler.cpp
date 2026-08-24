#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/Sampler.h"

using namespace Eppo;

TEST(Renderer, Sampler_OwnsItsBindlessHandle)
{
	if (!Testing::AppHarness::IsAvailable())
		return;

	const auto descriptorManager = CreateRef<DescriptorManager>();
	const auto sampler = Sampler::Create({}, descriptorManager);

	EXPECT_EQ(0u, sampler->GetBindlessIndex());
	EXPECT_EQ(1u, descriptorManager->GetSamplerHeap()->NextFreeSlot);
}

TEST(Renderer, Sampler_CreateAppliesIndependentAddressModes)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const auto descriptorManager = CreateRef<DescriptorManager>();
    const auto sampler = Sampler::Create(
        {
            .AddressModeU = nvrhi::SamplerAddressMode::Wrap,
            .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
            .AddressModeW = nvrhi::SamplerAddressMode::Mirror,
            .MinFilter = false,
            .MagFilter = false,
            .MipFilter = false,
        },
        descriptorManager
    );
    const auto& desc = sampler->GetSampler()->getDesc();

    EXPECT_TRUE(desc.addressU == nvrhi::SamplerAddressMode::Wrap);
    EXPECT_TRUE(desc.addressV == nvrhi::SamplerAddressMode::Clamp);
    EXPECT_TRUE(desc.addressW == nvrhi::SamplerAddressMode::Mirror);
    EXPECT_TRUE(!desc.minFilter);
    EXPECT_TRUE(!desc.magFilter);
    EXPECT_TRUE(!desc.mipFilter);
}
