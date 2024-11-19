#include "Test.h"

// TODO: Fixed random number seed?
namespace Eppo
{
	class UUIDTestFixture : public testing::TestWithParam<uint64_t>
	{};

	INSTANTIATE_TEST_SUITE_P(UUIDTest, UUIDTestFixture,
		testing::Values(4, 20, 1024, 2024));

	TEST(UUIDTest, Constructor)
	{
		UUID uuid;

		EXPECT_LT(0, uuid);
	}

	TEST(UUIDTest, Constructor_Zero)
	{
		UUID uuid(0);

		EXPECT_EQ(0, uuid);
	}

	TEST_P(UUIDTestFixture, Constructor_UInt64)
	{
		UUID uuid(GetParam());

		EXPECT_EQ(GetParam(), uuid);
	}

	TEST_P(UUIDTestFixture, Constructor_Copy)
	{
		UUID uuid(GetParam());
		UUID targetUuid(uuid);

		EXPECT_EQ(uuid, targetUuid);
	}
}
