#include "Support/EppoTest.h"
#include "Support/GlmCheck.h"
#include "Asset/Asset.h"
#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/SimulatedInput.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scripting/ScriptEngine.h"

#include <algorithm>
#include <ranges>

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
    }

    // --- Class metadata: the runtime comes up and reflects the user class. ---

    TEST(ScriptEngine_Init_DiscoversClasses)
    {
        REQUIRE CHECK(EnsureRuntime());
        CHECK(!ScriptEngine::Get().GetClasses().empty());
    }

    TEST(ScriptEngine_UserClass_IsDiscovered)
    {
        REQUIRE CHECK(EnsureRuntime());
        CHECK(ScriptEngine::Get().IsValidScriptClass(kUserClass));
    }

    TEST(ScriptEngine_EntityBaseClass_NotRegistered)
    {
        REQUIRE CHECK(EnsureRuntime());
        CHECK(!ScriptEngine::Get().IsValidScriptClass("EppoScriptCore.Scene.Entity"));
        CHECK_EQUAL(-1, ScriptEngine::Get().FindClassIndex("EppoScriptCore.Scene.Entity"));
    }

    TEST(ScriptClass_PublicFields_AreReflected)
    {
        REQUIRE CHECK(EnsureRuntime());
        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        const auto& fields = c->GetFields();
        const auto speed = std::ranges::find_if(fields, [](const ScriptField& f) { return f.Name == "Speed"; });
        REQUIRE CHECK(speed != fields.end());
        CHECK(speed->Type == ScriptFieldType::Float);
    }

    TEST(ScriptClass_PublicMethods_AreReflected)
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
    TEST(ScriptEngine_UnknownClass_ProducesNoInstance)
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
    TEST(ScriptEngine_CreateAndUpdate_RunManagedCode)
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
    TEST(ScriptEngine_DestroyEntity_UnregistersInstance)
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

    TEST(ScriptClass_InvokeMethod_MarshalsArgsAndReturn)
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
    TEST(ScriptClass_ManagedException_DoesNotCrashHost)
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

    TEST(Entity_NullEquality_IsSafe)
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

    TEST(ScriptInstance_IntField_MarshalsValue)
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
    TEST(ScriptInstance_Vector3Field_IsTypedAndMarshals)
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
    TEST(ScriptInstance_EntityField_IsTypedAndMarshals)
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
    TEST(ScriptInstance_BoolAndDoubleFields_Marshal)
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
    TEST(ScriptEngine_EditorFieldValues_PushedOnCreate)
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
    TEST(ScriptEngine_LiveFieldEdits_DiscardedOnReplay)
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

    TEST(LogMessage_NativeCallback_DoesNotCrashHost)
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
        const ScriptMethod* getTranslation = c->GetMethod("TransformComponent_GetTranslation");
        REQUIRE CHECK(getTranslation != nullptr);

        glm::vec3 translation{};
        c->InvokeMethod(entity, *getTranslation, nullptr, &translation);
        CHECK_VEC3_CLOSE(glm::vec3(1.0f, 2.0f, 3.0f), translation, 1e-5f);

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
        const ScriptMethod* getColor = c->GetMethod("PointLightComponent_GetColor");
        REQUIRE CHECK(getColor != nullptr);

        glm::vec3 color{};
        c->InvokeMethod(entity, *getColor, nullptr, &color);
        CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), color, 1e-5f);

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

    TEST(RigidBodyComponent_GetType_ReturnsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        entity.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Kinematic;

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* getType = c->GetMethod("RigidBodyComponent_GetType");
        REQUIRE CHECK(getType != nullptr);

        uint8_t got = 0;
        c->InvokeMethod(entity, *getType, nullptr, &got);
        CHECK_EQUAL(static_cast<int>(RigidBodyComponent::BodyType::Kinematic), static_cast<int>(got));

        engine.OnDestroyEntity(entity);
    }

    TEST(RigidBodyComponent_SetType_MutatesScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& rb = entity.AddComponent<RigidBodyComponent>();
        rb.Type = RigidBodyComponent::BodyType::Kinematic;

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* setType = c->GetMethod("RigidBodyComponent_SetType");
        REQUIRE CHECK(setType != nullptr);

        const uint8_t dynamic = static_cast<uint8_t>(RigidBodyComponent::BodyType::Dynamic);
        c->InvokeMethod(entity, *setType, &dynamic, nullptr);
        CHECK(rb.Type == RigidBodyComponent::BodyType::Dynamic);

        engine.OnDestroyEntity(entity);
    }

    TEST(BoxColliderComponent_Getters_ReturnSceneValues)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& box = entity.AddComponent<BoxColliderComponent>();
        box.HalfSize = glm::vec3(1.0f, 2.0f, 3.0f);
        box.Offset = glm::vec3(0.1f, 0.2f, 0.3f);
        box.Density = 2.0f;
        box.Friction = 0.25f;
        box.Restitution = 0.1f;

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        glm::vec3 halfSize{};
        const ScriptMethod* getHalfSize = c->GetMethod("BoxColliderComponent_GetHalfSize");
        REQUIRE CHECK(getHalfSize != nullptr);
        c->InvokeMethod(entity, *getHalfSize, nullptr, &halfSize);
        CHECK_VEC3_CLOSE(glm::vec3(1.0f, 2.0f, 3.0f), halfSize, 1e-5f);

        glm::vec3 offset{};
        const ScriptMethod* getOffset = c->GetMethod("BoxColliderComponent_GetOffset");
        REQUIRE CHECK(getOffset != nullptr);
        c->InvokeMethod(entity, *getOffset, nullptr, &offset);
        CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), offset, 1e-5f);

        float density = 0.0f;
        const ScriptMethod* getDensity = c->GetMethod("BoxColliderComponent_GetDensity");
        REQUIRE CHECK(getDensity != nullptr);
        c->InvokeMethod(entity, *getDensity, nullptr, &density);
        CHECK_CLOSE(2.0f, density, 1e-5f);

        float friction = 0.0f;
        const ScriptMethod* getFriction = c->GetMethod("BoxColliderComponent_GetFriction");
        REQUIRE CHECK(getFriction != nullptr);
        c->InvokeMethod(entity, *getFriction, nullptr, &friction);
        CHECK_CLOSE(0.25f, friction, 1e-5f);

        float restitution = 0.0f;
        const ScriptMethod* getRestitution = c->GetMethod("BoxColliderComponent_GetRestitution");
        REQUIRE CHECK(getRestitution != nullptr);
        c->InvokeMethod(entity, *getRestitution, nullptr, &restitution);
        CHECK_CLOSE(0.1f, restitution, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(BoxColliderComponent_Setters_MutateScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& box = entity.AddComponent<BoxColliderComponent>();

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        const glm::vec3 halfSize(4.0f, 5.0f, 6.0f);
        const ScriptMethod* setHalfSize = c->GetMethod("BoxColliderComponent_SetHalfSize");
        REQUIRE CHECK(setHalfSize != nullptr);
        c->InvokeMethod(entity, *setHalfSize, &halfSize, nullptr);
        CHECK_VEC3_CLOSE(halfSize, box.HalfSize, 1e-5f);

        const glm::vec3 offset(0.4f, 0.5f, 0.6f);
        const ScriptMethod* setOffset = c->GetMethod("BoxColliderComponent_SetOffset");
        REQUIRE CHECK(setOffset != nullptr);
        c->InvokeMethod(entity, *setOffset, &offset, nullptr);
        CHECK_VEC3_CLOSE(offset, box.Offset, 1e-5f);

        const float density = 3.0f;
        const ScriptMethod* setDensity = c->GetMethod("BoxColliderComponent_SetDensity");
        REQUIRE CHECK(setDensity != nullptr);
        c->InvokeMethod(entity, *setDensity, &density, nullptr);
        CHECK_CLOSE(3.0f, box.Density, 1e-5f);

        const float friction = 0.75f;
        const ScriptMethod* setFriction = c->GetMethod("BoxColliderComponent_SetFriction");
        REQUIRE CHECK(setFriction != nullptr);
        c->InvokeMethod(entity, *setFriction, &friction, nullptr);
        CHECK_CLOSE(0.75f, box.Friction, 1e-5f);

        const float restitution = 0.9f;
        const ScriptMethod* setRestitution = c->GetMethod("BoxColliderComponent_SetRestitution");
        REQUIRE CHECK(setRestitution != nullptr);
        c->InvokeMethod(entity, *setRestitution, &restitution, nullptr);
        CHECK_CLOSE(0.9f, box.Restitution, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(SphereColliderComponent_Getters_ReturnSceneValues)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& sphere = entity.AddComponent<SphereColliderComponent>();
        sphere.Radius = 1.5f;
        sphere.Offset = glm::vec3(0.1f, 0.2f, 0.3f);
        sphere.Density = 2.0f;
        sphere.Friction = 0.25f;
        sphere.Restitution = 0.1f;

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        float radius = 0.0f;
        const ScriptMethod* getRadius = c->GetMethod("SphereColliderComponent_GetRadius");
        REQUIRE CHECK(getRadius != nullptr);
        c->InvokeMethod(entity, *getRadius, nullptr, &radius);
        CHECK_CLOSE(1.5f, radius, 1e-5f);

        glm::vec3 offset{};
        const ScriptMethod* getOffset = c->GetMethod("SphereColliderComponent_GetOffset");
        REQUIRE CHECK(getOffset != nullptr);
        c->InvokeMethod(entity, *getOffset, nullptr, &offset);
        CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), offset, 1e-5f);

        float density = 0.0f;
        const ScriptMethod* getDensity = c->GetMethod("SphereColliderComponent_GetDensity");
        REQUIRE CHECK(getDensity != nullptr);
        c->InvokeMethod(entity, *getDensity, nullptr, &density);
        CHECK_CLOSE(2.0f, density, 1e-5f);

        float friction = 0.0f;
        const ScriptMethod* getFriction = c->GetMethod("SphereColliderComponent_GetFriction");
        REQUIRE CHECK(getFriction != nullptr);
        c->InvokeMethod(entity, *getFriction, nullptr, &friction);
        CHECK_CLOSE(0.25f, friction, 1e-5f);

        float restitution = 0.0f;
        const ScriptMethod* getRestitution = c->GetMethod("SphereColliderComponent_GetRestitution");
        REQUIRE CHECK(getRestitution != nullptr);
        c->InvokeMethod(entity, *getRestitution, nullptr, &restitution);
        CHECK_CLOSE(0.1f, restitution, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(SphereColliderComponent_Setters_MutateScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& sphere = entity.AddComponent<SphereColliderComponent>();

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        const float radius = 2.5f;
        const ScriptMethod* setRadius = c->GetMethod("SphereColliderComponent_SetRadius");
        REQUIRE CHECK(setRadius != nullptr);
        c->InvokeMethod(entity, *setRadius, &radius, nullptr);
        CHECK_CLOSE(2.5f, sphere.Radius, 1e-5f);

        const glm::vec3 offset(0.4f, 0.5f, 0.6f);
        const ScriptMethod* setOffset = c->GetMethod("SphereColliderComponent_SetOffset");
        REQUIRE CHECK(setOffset != nullptr);
        c->InvokeMethod(entity, *setOffset, &offset, nullptr);
        CHECK_VEC3_CLOSE(offset, sphere.Offset, 1e-5f);

        const float density = 3.0f;
        const ScriptMethod* setDensity = c->GetMethod("SphereColliderComponent_SetDensity");
        REQUIRE CHECK(setDensity != nullptr);
        c->InvokeMethod(entity, *setDensity, &density, nullptr);
        CHECK_CLOSE(3.0f, sphere.Density, 1e-5f);

        const float friction = 0.75f;
        const ScriptMethod* setFriction = c->GetMethod("SphereColliderComponent_SetFriction");
        REQUIRE CHECK(setFriction != nullptr);
        c->InvokeMethod(entity, *setFriction, &friction, nullptr);
        CHECK_CLOSE(0.75f, sphere.Friction, 1e-5f);

        const float restitution = 0.9f;
        const ScriptMethod* setRestitution = c->GetMethod("SphereColliderComponent_SetRestitution");
        REQUIRE CHECK(setRestitution != nullptr);
        c->InvokeMethod(entity, *setRestitution, &restitution, nullptr);
        CHECK_CLOSE(0.9f, sphere.Restitution, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(CapsuleColliderComponent_Getters_ReturnSceneValues)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& capsule = entity.AddComponent<CapsuleColliderComponent>();
        capsule.Radius = 1.5f;
        capsule.Height = 3.0f;
        capsule.Offset = glm::vec3(0.1f, 0.2f, 0.3f);
        capsule.Density = 2.0f;
        capsule.Friction = 0.25f;
        capsule.Restitution = 0.1f;

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        float radius = 0.0f;
        const ScriptMethod* getRadius = c->GetMethod("CapsuleColliderComponent_GetRadius");
        REQUIRE CHECK(getRadius != nullptr);
        c->InvokeMethod(entity, *getRadius, nullptr, &radius);
        CHECK_CLOSE(1.5f, radius, 1e-5f);

        float height = 0.0f;
        const ScriptMethod* getHeight = c->GetMethod("CapsuleColliderComponent_GetHeight");
        REQUIRE CHECK(getHeight != nullptr);
        c->InvokeMethod(entity, *getHeight, nullptr, &height);
        CHECK_CLOSE(3.0f, height, 1e-5f);

        glm::vec3 offset{};
        const ScriptMethod* getOffset = c->GetMethod("CapsuleColliderComponent_GetOffset");
        REQUIRE CHECK(getOffset != nullptr);
        c->InvokeMethod(entity, *getOffset, nullptr, &offset);
        CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), offset, 1e-5f);

        float density = 0.0f;
        const ScriptMethod* getDensity = c->GetMethod("CapsuleColliderComponent_GetDensity");
        REQUIRE CHECK(getDensity != nullptr);
        c->InvokeMethod(entity, *getDensity, nullptr, &density);
        CHECK_CLOSE(2.0f, density, 1e-5f);

        float friction = 0.0f;
        const ScriptMethod* getFriction = c->GetMethod("CapsuleColliderComponent_GetFriction");
        REQUIRE CHECK(getFriction != nullptr);
        c->InvokeMethod(entity, *getFriction, nullptr, &friction);
        CHECK_CLOSE(0.25f, friction, 1e-5f);

        float restitution = 0.0f;
        const ScriptMethod* getRestitution = c->GetMethod("CapsuleColliderComponent_GetRestitution");
        REQUIRE CHECK(getRestitution != nullptr);
        c->InvokeMethod(entity, *getRestitution, nullptr, &restitution);
        CHECK_CLOSE(0.1f, restitution, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(CapsuleColliderComponent_Setters_MutateScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& capsule = entity.AddComponent<CapsuleColliderComponent>();

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        const float radius = 2.5f;
        const ScriptMethod* setRadius = c->GetMethod("CapsuleColliderComponent_SetRadius");
        REQUIRE CHECK(setRadius != nullptr);
        c->InvokeMethod(entity, *setRadius, &radius, nullptr);
        CHECK_CLOSE(2.5f, capsule.Radius, 1e-5f);

        const float height = 4.0f;
        const ScriptMethod* setHeight = c->GetMethod("CapsuleColliderComponent_SetHeight");
        REQUIRE CHECK(setHeight != nullptr);
        c->InvokeMethod(entity, *setHeight, &height, nullptr);
        CHECK_CLOSE(4.0f, capsule.Height, 1e-5f);

        const glm::vec3 offset(0.4f, 0.5f, 0.6f);
        const ScriptMethod* setOffset = c->GetMethod("CapsuleColliderComponent_SetOffset");
        REQUIRE CHECK(setOffset != nullptr);
        c->InvokeMethod(entity, *setOffset, &offset, nullptr);
        CHECK_VEC3_CLOSE(offset, capsule.Offset, 1e-5f);

        const float density = 3.0f;
        const ScriptMethod* setDensity = c->GetMethod("CapsuleColliderComponent_SetDensity");
        REQUIRE CHECK(setDensity != nullptr);
        c->InvokeMethod(entity, *setDensity, &density, nullptr);
        CHECK_CLOSE(3.0f, capsule.Density, 1e-5f);

        const float friction = 0.75f;
        const ScriptMethod* setFriction = c->GetMethod("CapsuleColliderComponent_SetFriction");
        REQUIRE CHECK(setFriction != nullptr);
        c->InvokeMethod(entity, *setFriction, &friction, nullptr);
        CHECK_CLOSE(0.75f, capsule.Friction, 1e-5f);

        const float restitution = 0.9f;
        const ScriptMethod* setRestitution = c->GetMethod("CapsuleColliderComponent_SetRestitution");
        REQUIRE CHECK(setRestitution != nullptr);
        c->InvokeMethod(entity, *setRestitution, &restitution, nullptr);
        CHECK_CLOSE(0.9f, capsule.Restitution, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(CylinderColliderComponent_GettersAndSetters_RoundTripSceneValues)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& cylinder = entity.AddComponent<CylinderColliderComponent>();
        cylinder.Radius = 1.5f;
        cylinder.Height = 3.0f;
        cylinder.Offset = glm::vec3(0.1f, 0.2f, 0.3f);
        cylinder.Density = 2.0f;
        cylinder.Friction = 0.25f;
        cylinder.Restitution = 0.1f;

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        float radius = 0.0f;
        const ScriptMethod* getRadius = c->GetMethod("CylinderColliderComponent_GetRadius");
        REQUIRE CHECK(getRadius != nullptr);
        c->InvokeMethod(entity, *getRadius, nullptr, &radius);
        CHECK_CLOSE(1.5f, radius, 1e-5f);

        float height = 0.0f;
        const ScriptMethod* getHeight = c->GetMethod("CylinderColliderComponent_GetHeight");
        REQUIRE CHECK(getHeight != nullptr);
        c->InvokeMethod(entity, *getHeight, nullptr, &height);
        CHECK_CLOSE(3.0f, height, 1e-5f);

        glm::vec3 offset{};
        const ScriptMethod* getOffset = c->GetMethod("CylinderColliderComponent_GetOffset");
        REQUIRE CHECK(getOffset != nullptr);
        c->InvokeMethod(entity, *getOffset, nullptr, &offset);
        CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), offset, 1e-5f);

        float density = 0.0f;
        const ScriptMethod* getDensity = c->GetMethod("CylinderColliderComponent_GetDensity");
        REQUIRE CHECK(getDensity != nullptr);
        c->InvokeMethod(entity, *getDensity, nullptr, &density);
        CHECK_CLOSE(2.0f, density, 1e-5f);

        float friction = 0.0f;
        const ScriptMethod* getFriction = c->GetMethod("CylinderColliderComponent_GetFriction");
        REQUIRE CHECK(getFriction != nullptr);
        c->InvokeMethod(entity, *getFriction, nullptr, &friction);
        CHECK_CLOSE(0.25f, friction, 1e-5f);

        float restitution = 0.0f;
        const ScriptMethod* getRestitution = c->GetMethod("CylinderColliderComponent_GetRestitution");
        REQUIRE CHECK(getRestitution != nullptr);
        c->InvokeMethod(entity, *getRestitution, nullptr, &restitution);
        CHECK_CLOSE(0.1f, restitution, 1e-5f);

        radius = 2.5f;
        const ScriptMethod* setRadius = c->GetMethod("CylinderColliderComponent_SetRadius");
        REQUIRE CHECK(setRadius != nullptr);
        c->InvokeMethod(entity, *setRadius, &radius, nullptr);
        CHECK_CLOSE(2.5f, cylinder.Radius, 1e-5f);

        height = 4.0f;
        const ScriptMethod* setHeight = c->GetMethod("CylinderColliderComponent_SetHeight");
        REQUIRE CHECK(setHeight != nullptr);
        c->InvokeMethod(entity, *setHeight, &height, nullptr);
        CHECK_CLOSE(4.0f, cylinder.Height, 1e-5f);

        offset = glm::vec3(0.4f, 0.5f, 0.6f);
        const ScriptMethod* setOffset = c->GetMethod("CylinderColliderComponent_SetOffset");
        REQUIRE CHECK(setOffset != nullptr);
        c->InvokeMethod(entity, *setOffset, &offset, nullptr);
        CHECK_VEC3_CLOSE(offset, cylinder.Offset, 1e-5f);

        density = 3.0f;
        const ScriptMethod* setDensity = c->GetMethod("CylinderColliderComponent_SetDensity");
        REQUIRE CHECK(setDensity != nullptr);
        c->InvokeMethod(entity, *setDensity, &density, nullptr);
        CHECK_CLOSE(3.0f, cylinder.Density, 1e-5f);

        friction = 0.75f;
        const ScriptMethod* setFriction = c->GetMethod("CylinderColliderComponent_SetFriction");
        REQUIRE CHECK(setFriction != nullptr);
        c->InvokeMethod(entity, *setFriction, &friction, nullptr);
        CHECK_CLOSE(0.75f, cylinder.Friction, 1e-5f);

        restitution = 0.9f;
        const ScriptMethod* setRestitution = c->GetMethod("CylinderColliderComponent_SetRestitution");
        REQUIRE CHECK(setRestitution != nullptr);
        c->InvokeMethod(entity, *setRestitution, &restitution, nullptr);
        CHECK_CLOSE(0.9f, cylinder.Restitution, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    // LinearVelocity routes through the active physics world rather than the component.
    TEST(RigidBodyComponent_GetLinearVelocity_ReturnsWorldValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& rb = entity.AddComponent<RigidBodyComponent>();
        rb.Type = RigidBodyComponent::BodyType::Dynamic;

        const Ref<PhysicsWorld> world = CreateRef<PhysicsWorld>(glm::vec3(0.0f)); // gravity-free
        const auto& tc = entity.GetComponent<TransformComponent>();
        world->CreateBody(entity.GetUUID(), rb, tc.Translation, glm::quat(tc.Rotation), { ColliderData{} });
        world->SetLinearVelocity(entity.GetUUID(), glm::vec3(1.0f, 2.0f, 3.0f));
        engine.SetActivePhysicsWorld(world);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* getVelocity = c->GetMethod("RigidBodyComponent_GetLinearVelocity");
        REQUIRE CHECK(getVelocity != nullptr);

        glm::vec3 velocity{};
        c->InvokeMethod(entity, *getVelocity, nullptr, &velocity);
        CHECK_VEC3_CLOSE(glm::vec3(1.0f, 2.0f, 3.0f), velocity, 1e-4f);

        engine.OnDestroyEntity(entity);
    }

    TEST(RigidBodyComponent_SetLinearVelocity_MutatesWorld)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& rb = entity.AddComponent<RigidBodyComponent>();
        rb.Type = RigidBodyComponent::BodyType::Dynamic;

        const Ref<PhysicsWorld> world = CreateRef<PhysicsWorld>(glm::vec3(0.0f));
        const auto& tc = entity.GetComponent<TransformComponent>();
        world->CreateBody(entity.GetUUID(), rb, tc.Translation, glm::quat(tc.Rotation), { ColliderData{} });
        engine.SetActivePhysicsWorld(world);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* setVelocity = c->GetMethod("RigidBodyComponent_SetLinearVelocity");
        REQUIRE CHECK(setVelocity != nullptr);

        const glm::vec3 velocity(1.0f, 2.0f, 3.0f);
        c->InvokeMethod(entity, *setVelocity, &velocity, nullptr);
        CHECK_VEC3_CLOSE(velocity, world->GetLinearVelocity(entity.GetUUID()), 1e-4f);

        engine.OnDestroyEntity(entity);
    }

    // Physics.ApplyLinearImpulse (static) drives the body via the active world.
    TEST(Physics_ApplyLinearImpulse_AddsVelocity)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& rb = entity.AddComponent<RigidBodyComponent>();
        rb.Type = RigidBodyComponent::BodyType::Dynamic;

        const Ref<PhysicsWorld> world = CreateRef<PhysicsWorld>(glm::vec3(0.0f));
        const auto& tc = entity.GetComponent<TransformComponent>();
        world->CreateBody(entity.GetUUID(), rb, tc.Translation, glm::quat(tc.Rotation), { ColliderData{} });
        engine.SetActivePhysicsWorld(world);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* impulse = c->GetMethod("Physics_ApplyLinearImpulseUp");
        REQUIRE CHECK(impulse != nullptr);

        c->InvokeMethod(entity, *impulse, nullptr, nullptr);
        world->Step(1.0f / 60.0f);
        CHECK(world->GetLinearVelocity(entity.GetUUID()).y > 0.0f);

        engine.OnDestroyEntity(entity);
    }
}
