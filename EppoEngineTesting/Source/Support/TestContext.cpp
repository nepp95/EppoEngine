// pch first: this TU includes engine headers that name Ref<T> etc., and test
// TUs do not get the engine PCH automatically.
#include "pch.h"

#include "Support/TestContext.h"
#include "Support/AppHarness.h"
#include "Support/ScenarioLayer.h"

#include "Core/Input.h"

namespace Eppo::Testing
{
	namespace
	{
		// The scenario layer is pushed once into the harness app and lives for the
		// app's lifetime (the layer stack has no pop). Its update callback is
		// set/cleared per AdvanceFrames call, so only the active scenario's logic
		// runs. We key the cached pointer on the owning app instance so that if the
		// harness is ever torn down and re-booted (AppHarness::Shutdown re-arms
		// boot), we push a fresh layer into the new app rather than returning a
		// dangling pointer into the old one.
		auto GetScenarioLayer() -> ScenarioLayer*
		{
			static ScenarioLayer* layer = nullptr;
			static Application* owner = nullptr;

			Application* app = AppHarness::Get();
			if (!app)
				return nullptr;

			if (app != owner)
			{
				layer = app->PushLayer<ScenarioLayer>().get();
				owner = app;
			}

			return layer;
		}
	}

	TestContext::TestContext()
		: m_Scene(CreateRef<Scene>())
	{
		// Boot the shared application (no-op if already up) and route all input
		// queries through this scenario's SimulatedInput.
		//
		// Note: TestContext instances are not designed to nest — the destructor
		// restores the default device backend rather than a previous simulated
		// one. Scenarios each scope their own TestContext, so this is not a
		// concern in practice.
		(void)AppHarness::Get();
		Input::SetBackend(&m_Input);
	}

	TestContext::~TestContext()
	{
		// Restore the live device backend so later suites/tests are unaffected.
		Input::SetBackend(nullptr);
	}

	auto TestContext::IsAvailable() const -> bool
	{
		return AppHarness::IsAvailable();
	}

	auto TestContext::AdvanceFrames(uint32_t count, const std::function<void(float)>& perFrame, float timestep) -> void
	{
		if (!AppHarness::IsAvailable())
			return;

		// Drive the scenario's per-frame logic from inside the real frame loop:
		// StepFrame calls the ScenarioLayer's OnUpdate, which runs `perFrame`. The
		// logic therefore only executes if the app genuinely steps frames.
		ScenarioLayer* layer = GetScenarioLayer();
		if (layer)
			layer->SetUpdate(perFrame);

		AppHarness::AdvanceFrames(count, timestep);

		if (layer)
			layer->ClearUpdate();
	}
}
