#include "Support/EppoTest.h"

#include "Asset/Asset.h"
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
//
// The internal-call tests invoke thin 1:1 forwarders on HarnessScript (each named
// after the ScriptGlue internal call it forwards to) and assert only on native
// state / returned values, so each test targets one internal call — not the
// Entity/Component C# wrappers around it.
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

        // Like MakeLiveEntity, but also installs the scene as the engine's scene
        // context so the Entity/Component internal calls (ScriptGlue) can resolve
        // the entity's UUID back to a live entity. Needed by every script that
        // touches the component API.
        auto MakeContextEntity(const Ref<Scene>& scene) -> Entity
        {
            ScriptEngine::Get().SetSceneContext(scene);
            return MakeLiveEntity(scene);
        }

        // Invoke a no-arg harness method returning a Vector3 (12 bytes) into out.
        auto InvokeVec3(const Entity entity, const ScriptClass& c, const std::string& method, float out[3]) -> bool
        {
            const ScriptMethod* m = c.GetMethod(method);
            if (!m)
                return false;
            c.InvokeMethod(entity, *m, nullptr, out);
            return true;
        }
    }

    // --- Class metadata: the runtime comes up and reflects the user class. ---

    TEST(Metadata_RuntimeComesUpWithClasses)
    {
        REQUIRE CHECK(EnsureRuntime());
        CHECK(!ScriptEngine::Get().GetClasses().empty());
    }

    TEST(Metadata_UserClassIsDiscovered)
    {
        REQUIRE CHECK(EnsureRuntime());
        CHECK(ScriptEngine::Get().IsValidScriptClass(kUserClass));
    }

    TEST(Metadata_EntityBaseNotRegisteredAsScript)
    {
        REQUIRE CHECK(EnsureRuntime());
        CHECK(!ScriptEngine::Get().IsValidScriptClass("EppoScriptCore.Scene.Entity"));
        CHECK_EQUAL(-1, ScriptEngine::Get().FindClassIndex("EppoScriptCore.Scene.Entity"));
    }

    TEST(Metadata_PublicFieldsExposed)
    {
        REQUIRE CHECK(EnsureRuntime());
        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        const auto& fields = c->GetFields();
        const auto speed = std::ranges::find_if(fields, [](const ScriptField& f) { return f.Name == "Speed"; });
        REQUIRE CHECK(speed != fields.end());
        CHECK(speed->Type == ScriptFieldType::Float);
    }

    TEST(Metadata_PublicMethodsExposed)
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

    // An entity referencing an unknown class must not instantiate, and the
    // lifecycle calls around it must stay safe no-ops.
    TEST(Metadata_UnknownClassProducesNoInstance)
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

    // --- Instance lifecycle (ScriptGlue create/update/destroy). ---

    // OnCreate sets Created=1 and OnUpdate accumulates deltaTime on the harness;
    // reading those fields back proves both lifecycle calls ran managed code.
    TEST(Lifecycle_OnCreateAndUpdateRunManagedCode)
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

    // Stopping play (OnDestroyEntity) must remove the entity from the live
    // registry so GetEntityInstance no longer returns a handle.
    TEST(Lifecycle_DestroyUnregistersInstance)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeLiveEntity(scene);
        CHECK(engine.GetEntityInstance(entity.GetUUID()) != nullptr);

        engine.OnDestroyEntity(entity);
        CHECK(engine.GetEntityInstance(entity.GetUUID()) == nullptr);
    }

    // --- Method invocation marshalling (ScriptGlue InvokeMethod). ---

    TEST(Method_InvokeRoundTripsArgsAndReturn)
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

    // A managed exception thrown from user code must not escape the
    // UnmanagedCallersOnly boundary and fail-fast the host process.
    TEST(Method_ExceptionDoesNotCrashHost)
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

    TEST(Method_EntityNullEqualityIsSafe)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Scripted");
        entity.AddComponent<ScriptComponent>(std::string(kUserClass));

        auto& engine = ScriptEngine::Get();
        engine.OnCreateEntity(entity);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* nullSafe = c->GetMethod("NullEqualityIsSafe");
        REQUIRE CHECK(nullSafe != nullptr);

        bool safe = false;
        c->InvokeMethod(entity, *nullSafe, nullptr, &safe);
        CHECK_EQUAL(true, safe);

        engine.OnDestroyEntity(entity);
    }

    // --- Field marshalling (ScriptGlue Get/SetFieldValue). ---

    TEST(Field_ValueRoundTrips)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeLiveEntity(scene);

        ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
        REQUIRE CHECK(instance != nullptr);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const int32_t countIndex = FieldIndex(*c, "Count");
        REQUIRE CHECK(countIndex >= 0);

        int32_t value = 99;
        instance->SetFieldValue(countIndex, &value);
        int32_t readBack = 0;
        instance->GetFieldValue(countIndex, &readBack);
        CHECK_EQUAL(99, readBack);

        engine.OnDestroyEntity(entity);
    }

    // A Vector3 field must be reported with its real type (not None) and marshal
    // its 12 bytes both ways — regression cover for ManagedTypeToFieldType.
    TEST(Field_VectorIsTypedAndRoundTrips)
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
    TEST(Field_EntityIsTypedAndRoundTrips)
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
    TEST(Field_BoolAndDoubleRoundTrip)
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

    // Editor-time field values in the side table must be pushed into the fresh
    // managed instance when the entity's script is created on play.
    TEST(Field_EditorValuesPushedOnCreate)
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
    TEST(Field_LiveEditsDiscardedOnReplay)
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

    // --- Internal calls (ScriptGlue): each test invokes the 1:1 harness forwarder
    // for one internal call and asserts on native state / the returned value. ---
    TEST(Input_IsKeyPressed_ReturnsNativeState)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeLiveEntity(scene);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* isKeyPressed = c->GetMethod("Input_IsKeyPressed");
        REQUIRE CHECK(isKeyPressed != nullptr);

        SimulatedInput pressed;
        pressed.PressKey(Key::Space);
        Input::SetBackend(&pressed);
        bool down = false;
        c->InvokeMethod(entity, *isKeyPressed, nullptr, &down);
        CHECK_EQUAL(true, down);

        SimulatedInput released; // nothing pressed
        Input::SetBackend(&released);
        bool up = true;
        c->InvokeMethod(entity, *isKeyPressed, nullptr, &up);
        CHECK_EQUAL(false, up);

        Input::SetBackend(nullptr);
        engine.OnDestroyEntity(entity);
    }

    TEST(LogMessage_DoesNotCrashHost)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeLiveEntity(scene);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* logMessage = c->GetMethod("LogMessage");
        REQUIRE CHECK(logMessage != nullptr);

        c->InvokeMethod(entity, *logMessage, nullptr, nullptr);
        CHECK(true); // reached here: the native Log callback did not fault

        engine.OnDestroyEntity(entity);
    }

    TEST(Entity_HasComponent_ReflectsScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* hasComponent = c->GetMethod("Entity_HasComponent");
        REQUIRE CHECK(hasComponent != nullptr);

        bool before = true;
        c->InvokeMethod(entity, *hasComponent, nullptr, &before);
        CHECK_EQUAL(false, before);

        entity.AddComponent<PointLightComponent>();
        bool after = false;
        c->InvokeMethod(entity, *hasComponent, nullptr, &after);
        CHECK_EQUAL(true, after);

        engine.OnDestroyEntity(entity);
    }

    TEST(Entity_AddComponent_AddsToScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        CHECK(!entity.HasComponent<PointLightComponent>());

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* addComponent = c->GetMethod("Entity_AddComponent");
        REQUIRE CHECK(addComponent != nullptr);

        c->InvokeMethod(entity, *addComponent, nullptr, nullptr);
        CHECK(entity.HasComponent<PointLightComponent>());

        engine.OnDestroyEntity(entity);
    }

    TEST(Entity_RemoveComponent_RemovesFromScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        entity.AddComponent<PointLightComponent>();

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* removeComponent = c->GetMethod("Entity_RemoveComponent");
        REQUIRE CHECK(removeComponent != nullptr);

        bool removed = false;
        c->InvokeMethod(entity, *removeComponent, nullptr, &removed);
        CHECK_EQUAL(true, removed);
        CHECK(!entity.HasComponent<PointLightComponent>());

        engine.OnDestroyEntity(entity);
    }

    TEST(Entity_GetName_MarshalsStringToManaged)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        // The harness forwarder compares the native name against this exact string.
        entity.GetComponent<TagComponent>().Tag = "NamedEntity";

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* getName = c->GetMethod("Entity_GetName_Matches");
        REQUIRE CHECK(getName != nullptr);

        bool matches = false;
        c->InvokeMethod(entity, *getName, nullptr, &matches);
        CHECK_EQUAL(true, matches);

        // Negative case: a different name must decode differently, proving the
        // forwarder returns the real marshalled string rather than a constant.
        entity.GetComponent<TagComponent>().Tag = "Other";
        bool mismatches = true;
        c->InvokeMethod(entity, *getName, nullptr, &mismatches);
        CHECK_EQUAL(false, mismatches);

        engine.OnDestroyEntity(entity);
    }

    TEST(TransformComponent_GetTranslation_ReturnsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        entity.GetComponent<TransformComponent>().Translation = glm::vec3(1.0f, 2.0f, 3.0f);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        float out[3] = { 0.0f, 0.0f, 0.0f };
        REQUIRE CHECK(InvokeVec3(entity, *c, "TransformComponent_GetTranslation", out));
        CHECK_CLOSE(1.0f, out[0], 1e-5f);
        CHECK_CLOSE(2.0f, out[1], 1e-5f);
        CHECK_CLOSE(3.0f, out[2], 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(TransformComponent_SetTranslation_MutatesScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* setTranslation = c->GetMethod("TransformComponent_SetTranslation");
        REQUIRE CHECK(setTranslation != nullptr);

        const float in[3] = { 4.0f, 5.0f, 6.0f };
        c->InvokeMethod(entity, *setTranslation, in, nullptr);

        const glm::vec3 t = entity.GetComponent<TransformComponent>().Translation;
        CHECK_CLOSE(4.0f, t.x, 1e-5f);
        CHECK_CLOSE(5.0f, t.y, 1e-5f);
        CHECK_CLOSE(6.0f, t.z, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(MeshComponent_GetMeshHandle_ReturnsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        entity.AddComponent<MeshComponent>(AssetHandle(0x1234ull));

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* getHandle = c->GetMethod("MeshComponent_GetMeshHandle");
        REQUIRE CHECK(getHandle != nullptr);

        uint64_t handle = 0;
        c->InvokeMethod(entity, *getHandle, nullptr, &handle);
        CHECK(handle == 0x1234ull);

        engine.OnDestroyEntity(entity);
    }

    TEST(PointLightComponent_SetColor_MutatesScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        entity.AddComponent<PointLightComponent>();

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* setColor = c->GetMethod("PointLightComponent_SetColor");
        REQUIRE CHECK(setColor != nullptr);

        const float color[3] = { 0.25f, 0.5f, 0.75f };
        c->InvokeMethod(entity, *setColor, color, nullptr);

        const glm::vec3 stored = entity.GetComponent<PointLightComponent>().Color;
        CHECK_CLOSE(0.25f, stored.x, 1e-5f);
        CHECK_CLOSE(0.5f, stored.y, 1e-5f);
        CHECK_CLOSE(0.75f, stored.z, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(PointLightComponent_GetColor_ReturnsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        entity.AddComponent<PointLightComponent>().Color = glm::vec3(0.1f, 0.2f, 0.3f);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        float out[3] = { 0.0f, 0.0f, 0.0f };
        REQUIRE CHECK(InvokeVec3(entity, *c, "PointLightComponent_GetColor", out));
        CHECK_CLOSE(0.1f, out[0], 1e-5f);
        CHECK_CLOSE(0.2f, out[1], 1e-5f);
        CHECK_CLOSE(0.3f, out[2], 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(PointLightComponent_SetIntensity_MutatesScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        entity.AddComponent<PointLightComponent>();

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* setIntensity = c->GetMethod("PointLightComponent_SetIntensity");
        REQUIRE CHECK(setIntensity != nullptr);

        const float intensity = 4.5f;
        c->InvokeMethod(entity, *setIntensity, &intensity, nullptr);
        CHECK_CLOSE(4.5f, entity.GetComponent<PointLightComponent>().Intensity, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(PointLightComponent_GetIntensity_ReturnsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        entity.AddComponent<PointLightComponent>().Intensity = 7.25f;

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* getIntensity = c->GetMethod("PointLightComponent_GetIntensity");
        REQUIRE CHECK(getIntensity != nullptr);

        float intensity = 0.0f;
        c->InvokeMethod(entity, *getIntensity, nullptr, &intensity);
        CHECK_CLOSE(7.25f, intensity, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(RelationshipComponent_SetParent_MutatesScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity parent = MakeContextEntity(scene);
        Entity child = MakeContextEntity(scene); // CreateEntity gives it a RelationshipComponent

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* setParent = c->GetMethod("RelationshipComponent_SetParent");
        REQUIRE CHECK(setParent != nullptr);

        const uint64_t parentId = static_cast<uint64_t>(parent.GetUUID());
        c->InvokeMethod(child, *setParent, &parentId, nullptr);

        CHECK(child.GetComponent<RelationshipComponent>().Parent == parent.GetUUID());

        engine.OnDestroyEntity(child);
        engine.OnDestroyEntity(parent);
    }

    TEST(RelationshipComponent_GetParent_ReturnsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity parent = MakeContextEntity(scene);
        Entity child = MakeContextEntity(scene);
        scene->SetParent(child, parent);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* getParent = c->GetMethod("RelationshipComponent_GetParent");
        REQUIRE CHECK(getParent != nullptr);

        uint64_t parentId = 0;
        c->InvokeMethod(child, *getParent, nullptr, &parentId);
        CHECK(parentId == static_cast<uint64_t>(parent.GetUUID()));

        engine.OnDestroyEntity(child);
        engine.OnDestroyEntity(parent);
    }
}
