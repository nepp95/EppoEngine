#include "TestSupport/EppoTest.h"
#include "Core/Timer.h"

using Eppo::Timer;

TEST(Core, Timer_StartsOnConstruction)
{
    using namespace std::chrono;

    Timer timer;
    std::this_thread::sleep_for(2ms);
    EXPECT_LT(0, timer.GetElapsedMilliseconds());

    auto elapsed = timer.GetElapsedMilliseconds();
    std::this_thread::sleep_for(2ms);
    EXPECT_LT(elapsed, timer.GetElapsedMilliseconds());
}

TEST(Core, Timer_Reset)
{
    using namespace std::chrono;

    Timer timer;

    auto elapsedBegin = timer.GetElapsedMilliseconds();
    std::this_thread::sleep_for(2ms);

    auto elapsedEnd = timer.GetElapsedMilliseconds();
    EXPECT_GT(elapsedEnd, elapsedBegin);

    timer.Reset();
    EXPECT_NEAR(0, timer.GetElapsedMilliseconds(), 0.1f);
}
