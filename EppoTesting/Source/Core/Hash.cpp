#include "Test.h"

namespace Eppo
{
	TEST(HashTest, GenerateFnv_String)
	{
		uint64_t hash = 0;

		hash = Hash::GenerateFnv("abcde");
		EXPECT_EQ(12133862286523975779, hash);

		hash = Hash::GenerateFnv("12345");
		EXPECT_EQ(17908862145884878309, hash);

		hash = Hash::GenerateFnv("abcdefghijklmnopqrstuvwxyz1234567890!");
		EXPECT_EQ(14272954169027804443, hash);
	}

	TEST(HashTest, GenerateFnv_Buffer)
	{
		EXPECT_TRUE(false);
	}
}
