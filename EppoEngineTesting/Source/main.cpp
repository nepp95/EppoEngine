#include "TestSupport/EppoTest.h"

auto main(int argc, char** argv) -> int
{
    Eppo::Log::Init();

    testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();

    return result;
}
