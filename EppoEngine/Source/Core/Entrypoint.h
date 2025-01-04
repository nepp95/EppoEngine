#pragma once

#include "Core/Application.h"

int main(int argc, char** argv)
{
	Eppo::Log::Init();

	const Eppo::ApplicationCommandLineArgs args(argc, argv);

	Eppo::Application* app = CreateApplication(args);
	app->Run();

	delete app;
    return 0;
}
