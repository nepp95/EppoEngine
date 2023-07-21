#include <EppoEngine.h>
#include <Core/Entrypoint.h>

class Editor : public Eppo::Application
{
public:
	Editor(const Eppo::ApplicationSpecification& specification)
		: Eppo::Application(specification)
	{
		// TODO: Layers
	}

	~Editor() = default;
};

Application* Eppo::CreateApplication(ApplicationCommandLineArgs args)
{
	ApplicationSpecification spec;
	spec.Name = "EppoEditor";
	spec.CommandLineArgs = args;

	return new Editor(spec);
}
