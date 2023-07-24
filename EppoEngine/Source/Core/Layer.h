#pragma once

namespace Eppo
{
	class Layer
	{
	public:
		Layer(const std::string& name = "Layer");
		virtual ~Layer() = default;

		virtual void OnAttach() {};
		virtual void OnDetach() {};

		virtual void Update(float timestep) {};
		virtual void Render() {};

		// Events
		// GUI

	private:
		std::string m_DebugName;
	};
}