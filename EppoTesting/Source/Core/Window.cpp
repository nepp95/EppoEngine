#include "Test.h"

namespace Eppo
{
	TEST(WindowTest, ConstructorWithSpecification)
	{
		WindowSpecification specification
		{
			"Test Window",
			800,
			600,
			60,
			false
		};

		// TODO: We can't do this apparently because it needs a window handle....
		//
		//Window window(specification);
		//
		//EXPECT_EQ(window.GetRendererContext(), nullptr);
		//EXPECT_EQ(window.GetSpecification().Title, "Test Window");
		//EXPECT_EQ(window.GetSpecification().Width, 800);
		//EXPECT_EQ(window.GetSpecification().Height, 600);
		//EXPECT_EQ(window.GetSpecification().RefreshRate, 60);
		//EXPECT_EQ(window.GetSpecification().OverrideSpecification, false);
	}

	// TODO: Test fixture
	TEST(WindowTest, Init)
	{
		WindowSpecification specification
		{
			"Test Window",
			800,
			600,
			60,
			false
		};

		// TODO: We can't do this apparently because it needs a window handle....
		//
		//Window window(specification);
		//
		//window.Init();
		//
		//EXPECT_NE(window.GetRendererContext(), nullptr);
	}

	// TODO: Test fixture
	TEST(WindowTest, Shutdown)
	{
		WindowSpecification specification
		{
			"Test Window",
			800,
			600,
			60,
			false
		};
	}

	// TODO: Test fixture
	TEST(WindowTest, ProcessEvents)
	{
		WindowSpecification specification
		{
			"Test Window",
			800,
			600,
			60,
			false
		};
	}
}
