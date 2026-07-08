#pragma once

#include "Core/Layer.h"
#include "ImGui/ImGuiRenderer.h"

namespace Eppo
{
	class ImGuiLayer : public Layer
	{
	public:
		auto OnAttach() -> void override;
		auto OnDetach() -> void override;
		auto OnEvent(Event& e) -> void override;

		auto PrepareRender() -> void;
		auto Render() -> void;
		auto BlockEvents(bool blockEvents) -> void;
		[[nodiscard]] constexpr auto GetMainImGuiRenderer() const -> const ScopedPtr<ImGuiRenderer>& { return m_ImGuiRenderer; }

	private:
		auto InitPlatformInterface() -> void;
		static auto ImGuiRenderer_CreateWindow(ImGuiViewport* viewport) -> void;
		static auto ImGuiRenderer_DestroyWindow(ImGuiViewport* viewport) -> void;
		static auto ImGuiRenderer_SetWindowSize(ImGuiViewport* viewport, ImVec2 size) -> void;
		static auto ImGuiRenderer_RenderWindow(ImGuiViewport* viewport, void*) -> void;
		static auto ImGuiRenderer_SwapBuffers(ImGuiViewport* viewport, void*) -> void;

	private:
		ScopedPtr<ImGuiRenderer> m_ImGuiRenderer = nullptr;
		bool m_BlockEvents = false;
	};
}