#pragma once

#include "Core/Base.h"
#include "Core/Application.h"

auto main(const int argc, char** argv) -> int
{
	// Initialize logging
	Eppo::Log::Init();

	// Create application
	Eppo::Application* app = Eppo::CreateApplication(argc, argv);
	EP_ASSERT(app, "Application could not be created!");

	// Run application
	app->Run();

	// Cleanup
	delete app;
	return 0;
}