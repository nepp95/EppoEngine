#include "Support/EppoTest.h"

#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/SimulatedInput.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scripting/ScriptEngine.h"

#include <algorithm>

using namespace Eppo;

// Boots the hosted runtime from the provisioned managed core + runtimeconfig and
// loads the tiny user assembly (EppoTesting.Scripts.dll). CoreCLR can't be
// re-initialised per-process, so the whole suite shares one Init + load.
SUITE(Scripting)
{
    constexpr const char* kUserClass = "EppoTesting.HarnessScript";

    namespace
    {
        auto EnsureRuntime() -> bool
        {
            static const bool ready = []
            {
                try
                {
                    if (!ScriptEngine::Init(FS::GetRootDirectory() / "runtimeconfig.json"))
                        return false;
                    if (!ScriptEngine::Get().IsRuntimeLoaded())
                        return false;
                    ScriptEngine::Get().LoadUserAssembly(FS::GetRootDirectory() / "EppoTesting.Scripts.dll");
                    // The managed side swallows a failed load (missing/bad assembly),
                    // so confirm the harness class is really present — otherwise a
                    // provisioning gap would surface as opaque per-test failures.
                    return ScriptEngine::Get().IsValidScriptClass(kUserClass);
                }
                catch (...) { return false; }
            }();
            return ready;
        }

        auto FindClass(const std::string& fullName) -> const ScriptClass*
        {
            const auto& classes = ScriptEngine::Get().GetClasses();
            const auto it = std::ranges::find_if(classes,
                [&](const ScriptClass& c) { return c.GetFullName() == fullName; });
            return it == classes.end() ? nullptr : &*it;
        }

        // Index of a field by name within a class's GetFields(); -1 if absent.
        auto FieldIndex(const ScriptClass& c, const std::string& name) -> int32_t
        {
            const auto& fields = c.GetFields();
            for (int32_t i = 0; i < static_cast<int32_t>(fields.size()); i++)
                if (fields[i].Name == name)
                    return i;
            return -1;
        }

        // A live, playing script instance for the harness class on a fresh entity.
        // Keeps the owning scene alive via the out-param so the entity stays valid.
        auto MakeLiveEntity(const Ref<Scene>& scene) -> Entity
        {
            Entity entity = scene->CreateEntity("Scripted");
            entity.AddComponent<ScriptComponent>(std::string(kUserClass));
            ScriptEngine::Get().OnCreateEntity(entity);
            return entity;
        }
    }

    TEST(RuntimeComesUpWithClassMetadata)
    {
        REQUIRE CHECK(EnsureRuntime());
        CHECK(!ScriptEngine::Get().GetClasses().empty());
    }

    TEST(UserAssemblyExposesItsClass)
    {
        REQUIRE CHECK(EnsureRuntime());
        CHECK(ScriptEngine::Get().IsValidScriptClass(kUserClass));
    }

    TEST(ScriptClassExposesPublicFields)
    {
        REQUIRE CHECK(EnsureRuntime());
        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        const auto& fields = c->GetFields();
        const auto speed = std::ranges::find_if(fields, [](const ScriptField& f) { return f.Name == "Speed"; });
        REQUIRE CHECK(speed != fields.end());
        CHECK(speed->Type == ScriptFieldType::Float);
    }

    TEST(ScriptClassExposesPublicMethods)
    {
        REQUIRE CHECK(EnsureRuntime());
        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        const ScriptMethod* add = c->GetMethod("Add");
        REQUIRE CHECK(add != nullptr);
        CHECK_EQUAL("Add", add->Name);
        CHECK(add->Index >= 0);
        CHECK(c->GetMethod("DoesNotExist") == nullptr);
    }

    TEST(InvokeMethodRoundTripsArgsAndReturn)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Scripted");
        entity.AddComponent<ScriptComponent>(std::string(kUserClass));

        auto& engine = ScriptEngine::Get();
        engine.OnCreateEntity(entity);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* add = c->GetMethod("Add");
        REQUIRE CHECK(add != nullptr);

        const int32_t args[2] = { 20, 22 }; // packed back-to-back
        int32_t result = 0;
        c->InvokeMethod(entity, *add, args, &result);
        CHECK_EQUAL(42, result);

        engine.OnDestroyEntity(entity);
    }

    TEST(ScriptCallbacksRouteBackIntoTheEngine)
    {
        REQUIRE CHECK(EnsureRuntime());

        // The default Input backend needs a live Application; install a simulated
        // one. Space is pressed, so a returned 1 proves the Input internal call
        // routed to native (a null/unregistered pointer would report 0).
        SimulatedInput input;
        input.PressKey(Key::Space);
        Input::SetBackend(&input);

        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Scripted");
        entity.AddComponent<ScriptComponent>(std::string(kUserClass));

        auto& engine = ScriptEngine::Get();
        engine.OnCreateEntity(entity);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* probe = c->GetMethod("Probe");
        REQUIRE CHECK(probe != nullptr);

        int32_t result = -1;
        c->InvokeMethod(entity, *probe, nullptr, &result);
        CHECK_EQUAL(1, result);

        engine.OnDestroyEntity(entity);
        Input::SetBackend(nullptr);
    }

    TEST(LiveInstanceFieldValueRoundTrips)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Scripted");
        entity.AddComponent<ScriptComponent>(std::string(kUserClass));

        auto& engine = ScriptEngine::Get();
        engine.OnCreateEntity(entity);

        ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
        REQUIRE CHECK(instance != nullptr);

        // Find the index of the int field "Count" on the class.
        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const auto& fields = c->GetFields();
        int32_t countIndex = -1;
        for (int32_t i = 0; i < static_cast<int32_t>(fields.size()); i++)
            if (fields[i].Name == "Count") countIndex = i;
        REQUIRE CHECK(countIndex >= 0);

        int32_t value = 99;
        instance->SetFieldValue(countIndex, &value);
        int32_t readBack = 0;
        instance->GetFieldValue(countIndex, &readBack);
        CHECK_EQUAL(99, readBack);

        engine.OnDestroyEntity(entity);
    }

    TEST(ScriptExceptionDoesNotCrashHost)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Scripted");
        entity.AddComponent<ScriptComponent>(std::string(kUserClass));

        auto& engine = ScriptEngine::Get();
        engine.OnCreateEntity(entity);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* throws = c->GetMethod("Throws");
        REQUIRE CHECK(throws != nullptr);

        int32_t result = -1;
        c->InvokeMethod(entity, *throws, nullptr, &result);

        // Reaching here proves the managed exception did not fail-fast the host process.
        CHECK(true);

        engine.OnDestroyEntity(entity);
    }

    // OnCreate sets Created=1 and OnUpdate accumulates deltaTime on the harness;
    // reading those fields back proves both lifecycle calls ran managed code.
    TEST(LifecycleCreateAndUpdateRunManagedCode)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeLiveEntity(scene);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
        REQUIRE CHECK(instance != nullptr);

        int32_t created = 0;
        instance->GetFieldValue(FieldIndex(*c, "Created"), &created);
        CHECK_EQUAL(1, created);

        engine.OnUpdateEntity(entity, 0.5f);
        engine.OnUpdateEntity(entity, 0.25f);

        float accumulated = 0.0f;
        instance->GetFieldValue(FieldIndex(*c, "Accumulated"), &accumulated);
        CHECK_CLOSE(0.75f, accumulated, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    // Editor-time field values in the side table must be pushed into the fresh
    // managed instance when the entity's script is created on play.
    TEST(EditorFieldsPushedIntoLiveInstanceOnCreate)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = scene->CreateEntity("Scripted");
        entity.AddComponent<ScriptComponent>(std::string(kUserClass));

        ScriptFieldValue stored;
        stored.Type = ScriptFieldType::Float;
        stored.Set(9.0f);
        engine.GetFieldMap(entity.GetUUID())["Speed"] = stored;

        engine.OnCreateEntity(entity);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
        REQUIRE CHECK(instance != nullptr);

        float speed = 0.0f;
        instance->GetFieldValue(FieldIndex(*c, "Speed"), &speed);
        CHECK_CLOSE(9.0f, speed, 1e-5f);

        engine.OnDestroyEntity(entity);
        engine.RemoveFieldMap(entity.GetUUID());
    }

    // Editing a field during play mutates the live instance directly and bypasses
    // the side table, so stopping and replaying restores the editor-time value.
    TEST(LiveEditsAreDiscardedOnReplay)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = scene->CreateEntity("Scripted");
        entity.AddComponent<ScriptComponent>(std::string(kUserClass));

        ScriptFieldValue stored;
        stored.Type = ScriptFieldType::Float;
        stored.Set(3.0f);
        engine.GetFieldMap(entity.GetUUID())["Speed"] = stored;

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const int32_t speedIndex = FieldIndex(*c, "Speed");

        engine.OnCreateEntity(entity);
        {
            ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
            REQUIRE CHECK(instance != nullptr);
            float live = 50.0f;
            instance->SetFieldValue(speedIndex, &live); // edit during play
            float check = 0.0f;
            instance->GetFieldValue(speedIndex, &check);
            CHECK_CLOSE(50.0f, check, 1e-5f); // edit took on the live instance
        }
        engine.OnDestroyEntity(entity); // stop

        engine.OnCreateEntity(entity); // replay
        {
            ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
            REQUIRE CHECK(instance != nullptr);
            float restored = 0.0f;
            instance->GetFieldValue(speedIndex, &restored);
            CHECK_CLOSE(3.0f, restored, 1e-5f); // side-table value, live edit gone
        }
        engine.OnDestroyEntity(entity);
        engine.RemoveFieldMap(entity.GetUUID());
    }

    // A Vector3 field must be reported with its real type (not None) and marshal
    // its 12 bytes both ways — regression cover for ManagedTypeToFieldType.
    TEST(VectorFieldIsTypedAndRoundTrips)
    {
        REQUIRE CHECK(EnsureRuntime());

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const int32_t posIndex = FieldIndex(*c, "Position");
        REQUIRE CHECK(posIndex >= 0);
        CHECK(c->GetFields()[posIndex].Type == ScriptFieldType::Vector3);

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeLiveEntity(scene);
        ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
        REQUIRE CHECK(instance != nullptr);

        const float in[3] = { 1.5f, -2.0f, 3.25f };
        instance->SetFieldValue(posIndex, in);
        float out[3] = { 0.0f, 0.0f, 0.0f };
        instance->GetFieldValue(posIndex, out);
        CHECK_CLOSE(1.5f, out[0], 1e-5f);
        CHECK_CLOSE(-2.0f, out[1], 1e-5f);
        CHECK_CLOSE(3.25f, out[2], 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    // An Entity field (a managed reference type) must be typed as Entity and
    // marshal as its 8-byte id both ways.
    TEST(EntityFieldIsTypedAndRoundTrips)
    {
        REQUIRE CHECK(EnsureRuntime());

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const int32_t targetIndex = FieldIndex(*c, "Target");
        REQUIRE CHECK(targetIndex >= 0);
        CHECK(c->GetFields()[targetIndex].Type == ScriptFieldType::Entity);

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeLiveEntity(scene);
        ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
        REQUIRE CHECK(instance != nullptr);

        const uint64_t id = 0xABCDEF0123456789ull;
        instance->SetFieldValue(targetIndex, &id);
        uint64_t back = 0;
        instance->GetFieldValue(targetIndex, &back);
        CHECK(back == id);

        engine.OnDestroyEntity(entity);
    }

    // bool and double fields round-trip through the marshalling layer intact.
    TEST(BoolAndDoubleFieldsRoundTrip)
    {
        REQUIRE CHECK(EnsureRuntime());

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const int32_t enabledIndex = FieldIndex(*c, "Enabled");
        const int32_t ratioIndex = FieldIndex(*c, "Ratio");
        REQUIRE CHECK(enabledIndex >= 0);
        REQUIRE CHECK(ratioIndex >= 0);
        CHECK(c->GetFields()[enabledIndex].Type == ScriptFieldType::Bool);
        CHECK(c->GetFields()[ratioIndex].Type == ScriptFieldType::Double);

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeLiveEntity(scene);
        ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
        REQUIRE CHECK(instance != nullptr);

        bool enabled = false;
        instance->SetFieldValue(enabledIndex, &enabled);
        bool enabledBack = true;
        instance->GetFieldValue(enabledIndex, &enabledBack);
        CHECK_EQUAL(false, enabledBack);

        double ratio = 2.75;
        instance->SetFieldValue(ratioIndex, &ratio);
        double ratioBack = 0.0;
        instance->GetFieldValue(ratioIndex, &ratioBack);
        CHECK_CLOSE(2.75, ratioBack, 1e-9);

        engine.OnDestroyEntity(entity);
    }

    // An entity referencing an unknown class must not instantiate, and the
    // lifecycle calls around it must stay safe no-ops.
    TEST(UnknownScriptClassProducesNoInstance)
    {
        REQUIRE CHECK(EnsureRuntime());

        auto& engine = ScriptEngine::Get();
        CHECK(!engine.IsValidScriptClass("EppoTesting.NoSuchClass"));
        CHECK_EQUAL(-1, engine.FindClassIndex("EppoTesting.NoSuchClass"));

        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Bad");
        entity.AddComponent<ScriptComponent>(std::string("EppoTesting.NoSuchClass"));

        engine.OnCreateEntity(entity);
        CHECK(engine.GetEntityInstance(entity.GetUUID()) == nullptr);

        engine.OnUpdateEntity(entity, 0.016f); // no live instance: safe no-op
        engine.OnDestroyEntity(entity);        // no live instance: safe no-op
        CHECK(true);
    }

    // Stopping play (OnDestroyEntity) must remove the entity from the live
    // registry so GetEntityInstance no longer returns a handle.
    TEST(DestroyUnregistersLiveInstance)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeLiveEntity(scene);
        CHECK(engine.GetEntityInstance(entity.GetUUID()) != nullptr);

        engine.OnDestroyEntity(entity);
        CHECK(engine.GetEntityInstance(entity.GetUUID()) == nullptr);
    }
}
