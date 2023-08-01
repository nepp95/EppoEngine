#pragma once

namespace Eppo
{
	class Scene
	{
	public:
		Scene() = default;
		~Scene() = default;

		void OnUpdate(float timestep);
		void Render();

	private:

	};
}
