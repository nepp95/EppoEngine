#include "Support/EppoTest.h"

#include "Core/Hash.h"

using Eppo::Hash;

TEST(Core, Hash_GenerateFnv)
{
    auto n1 = Hash::GenerateFnv("4284920001551835734");
    EXPECT_EQ(8836777831029394659u, n1);

    auto n2 = Hash::GenerateFnv("4108780039757652933");
    EXPECT_EQ(10680228333383690660u, n2);

    auto n3 = Hash::GenerateFnv("1554287213803382329");
    EXPECT_EQ(10359703057953629803u, n3);
}

TEST(Core, Hash_GenerateFnv_SmallNumber)
{
    auto n = Hash::GenerateFnv("123");
    EXPECT_EQ(15835819676631461701u, n);
}

TEST(Core, Hash_GenerateFnv_BigValue)
{
    auto n = Hash::GenerateFnv(
        "dfhadoisahdjsadjdifhiahfdhaidshfiouhhiuafheuihfiueawfheiuwhfiheaiuwfhiuawhfiuheiwuofhaiuwefhiuoawfiouheiuwfhwaeiuhfiouawhfioud"
        "sahhfiouhdsjnmnwiuhdiasjnk"
    );
    EXPECT_EQ(9048482559141267579u, n);
}