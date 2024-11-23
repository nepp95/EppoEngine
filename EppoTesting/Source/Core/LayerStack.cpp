#include "Test.h"

namespace Eppo
{
	TEST(LayerStackTest, PushLayer)
	{
		LayerStack layerStack;
		Layer* layer = new Layer();

		ASSERT_EQ(layerStack.begin(), layerStack.end());
		ASSERT_EQ(layerStack.rbegin(), layerStack.rend());

		layerStack.PushLayer(layer);

		ASSERT_NE(layerStack.begin(), layerStack.end());
		ASSERT_NE(layerStack.rbegin(), layerStack.rend());
	}

	TEST(LayerStackTest, PopLayer)
	{
		LayerStack layerStack;
		Layer* layer = new Layer();

		ASSERT_EQ(layerStack.begin(), layerStack.end());
		ASSERT_EQ(layerStack.rbegin(), layerStack.rend());

		layerStack.PushLayer(layer);

		ASSERT_NE(layerStack.begin(), layerStack.end());
		ASSERT_NE(layerStack.rbegin(), layerStack.rend());

		layerStack.PopLayer(layer);

		ASSERT_EQ(layerStack.begin(), layerStack.end());
		ASSERT_EQ(layerStack.rbegin(), layerStack.rend());
	}

	TEST(LayerStackTest, PushOverlay)
	{
		LayerStack layerStack;
		Layer* layer = new Layer();

		ASSERT_EQ(layerStack.begin(), layerStack.end());
		ASSERT_EQ(layerStack.rbegin(), layerStack.rend());

		layerStack.PushOverlay(layer);

		ASSERT_NE(layerStack.begin(), layerStack.end());
		ASSERT_NE(layerStack.rbegin(), layerStack.rend());
	}

	TEST(LayerStackTest, PopOverlay)
	{
		LayerStack layerStack;
		Layer* layer = new Layer();

		ASSERT_EQ(layerStack.begin(), layerStack.end());
		ASSERT_EQ(layerStack.rbegin(), layerStack.rend());

		layerStack.PushOverlay(layer);

		ASSERT_NE(layerStack.begin(), layerStack.end());
		ASSERT_NE(layerStack.rbegin(), layerStack.rend());

		layerStack.PopOverlay(layer);

		ASSERT_EQ(layerStack.begin(), layerStack.end());
		ASSERT_EQ(layerStack.rbegin(), layerStack.rend());
	}
}
