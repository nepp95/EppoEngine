#include "EditorLayer.h"

#include <EppoEngine.h>
#include <Core/EntryPoint.h>

namespace Eppo
{
	class Editor final : public Application
	{
	public:
        explicit Editor(ApplicationParams&& params)
			: Application(std::move(params))
		{
			PushLayer<EditorLayer>();
		}
	};

	auto CreateApplication(const int argc, char** argv) -> Application*
	{
        const CommandLineArgs args(argc, argv);
		ApplicationParams params{
			.Args = args,
		};

		const auto app = new Editor(std::move(params));

		return app;
	}
}