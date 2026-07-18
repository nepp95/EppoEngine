#include "Support/EppoTest.h"

#include "Core/Hash.h"

using Eppo::Hash;

SUITE(Core)
{
    TEST(Hash_GenerateFnv)
    {
        auto n1 = Hash::GenerateFnv("4284920001551835734");
        CHECK_EQUAL(8836777831029394659u, n1);

        auto n2 = Hash::GenerateFnv("4108780039757652933");
        CHECK_EQUAL(10680228333383690660u, n2);

        auto n3 = Hash::GenerateFnv("1554287213803382329");
        CHECK_EQUAL(10359703057953629803u, n3);
    }

    TEST(Hash_GenerateFnv_SmallNumber)
    {
        auto n = Hash::GenerateFnv("123");
        CHECK_EQUAL(15835819676631461701u, n);
    }

    TEST(Hash_GenerateFnv_BigValue)
    {
        auto n = Hash::GenerateFnv("dfhadoisahdjsadjdifhiahfdhaidshfiouhhiuafheuihfiueawfheiuwhfiheaiuwfhiuawhfiuheiwuofhaiuwefhiuoawfiouheiuwfhwaeiuhfiouawhfioudsahhfiouhdsjnmnwiuhdiasjnk");
        CHECK_EQUAL(9048482559141267579u, n);
    }
}