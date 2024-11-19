#include "Test.h"

// TODO: Fixed random number seed?
namespace Eppo
{
	TEST(UUIDTest, Ctor)
	{
		UUID uuid;

		EXPECT_LT(0, uuid);
	}

	TEST(UUIDTest, Ctor_Zero)
	{
		UUID uuid(0);

		EXPECT_EQ(0, uuid);
	}

	TEST(UUIDTest, Ctor_UInt64)
	{
		UUID uuid(12345);

		EXPECT_EQ(12345, uuid);
	}

	TEST(UUIDTest, Ctor_Copy)
	{
		UUID uuid(12345);
		UUID targetUuid(uuid);

		EXPECT_EQ(uuid, targetUuid);
	}
}
