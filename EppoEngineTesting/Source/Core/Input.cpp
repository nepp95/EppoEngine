#include "TestSupport/EppoTest.h"
#include "Core/Input.h"

using Eppo::Input;

TEST(Core, Input_ViewportInputRoundtrip)
{
    EXPECT_TRUE(Input::IsViewportInputEnabled());
    Input::SetViewportInputEnabled(false);
    EXPECT_FALSE(Input::IsViewportInputEnabled());
    Input::SetViewportInputEnabled(true);
    EXPECT_TRUE(Input::IsViewportInputEnabled());
}
