#pragma once

#include "Core/Application.h"

using namespace Eppo;

int main(int argc, char** argv)
{
	Log::Init();

	ApplicationCommandLineArgs args(argc, argv);

	Application* app = CreateApplication(args);
	app->Run();

	delete app;
    return 0;
}