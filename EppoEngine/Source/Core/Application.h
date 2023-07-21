#pragma once

#include <string>

int main(int argc, char** argv);

namespace Eppo
{
	struct ApplicationCommandLineArgs
	{
		ApplicationCommandLineArgs() = default;
		ApplicationCommandLineArgs(int argc, char** argv)
			: Count(argc), Args(argv)
		{}

		int Count = 0;
		char** Args = nullptr;

		const char* operator[](int index) const
		{
			// TODO: Assert index < count
			return Args[index];
		}
	};

	struct ApplicationSpecification
	{
		std::string Name = "Client";
		std::string WorkingDirectory;

		uint32_t WindowWidth = 1280;
		uint32_t WindowHeight = 720;

		ApplicationCommandLineArgs CommandLineArgs;
	};

	class Application
	{
	public:
		Application(const ApplicationSpecification& specification);

	private:
		ApplicationSpecification m_Specification;

	private:
		void Run();

		friend int ::main(int argc, char** argv);
	};

	// To be implemented by the client using this library
	Application* CreateApplication(ApplicationCommandLineArgs args);
}