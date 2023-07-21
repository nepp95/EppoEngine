#pragma once

#include "Core/Application.h"

using namespace Eppo;

int main(int argc, char** argv)
{
	ApplicationCommandLineArgs args(argc, argv);

	Application* app = CreateApplication(args);
	app->Run();

	delete app;
    return 0;
}