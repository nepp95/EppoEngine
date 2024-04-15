#pragma once

namespace Eppo
{
	class ScriptGlue
	{
	public:
		static void RegisterFunctions();
		static void RegisterComponents();

	private:
		template<typename T>
		static void RegisterComponent();
	};
}
