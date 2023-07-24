#include <EppoEngine.h>
#include <Core/Entrypoint.h>

#include "EditorLayer.h"

namespace Eppo
{
	class Editor : public Application
	{
	public:
		Editor(const Eppo::ApplicationSpecification& specification)
			: Application(specification)
		{
			PushLayer(new EditorLayer());
		}
	
		~Editor() = default;
	};
	
	Application* CreateApplication(ApplicationCommandLineArgs args)
	{
		ApplicationSpecification spec;
		spec.Name = "EppoEditor";
		spec.CommandLineArgs = args;
	
		return new Editor(spec);
	}
}
