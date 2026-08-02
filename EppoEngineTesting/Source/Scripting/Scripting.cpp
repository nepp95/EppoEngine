#include "Support/EppoTest.h"
#include "Support/GlmCheck.h"
#include "Support/TempDir.h"
#include "Asset/Asset.h"
#include "Asset/AssetManager.h"
#include "Physics/PhysicsWorld.h"
#include "Project/Project.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
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
    constexpr const char* kLifecycleClass = "EppoTesting.LifecycleScript";

    namespace
    {
        auto EnsureRuntime() -> bool
        {
            static const bool ready = []
            {
                try
                {
                    if (!ScriptEngine::Init(FS::GetExecutableDirectory() / "runtimeconfig.json"))
                        return false;
                    if (!ScriptEngine::Get().IsRuntimeLoaded())
                        return false;
                    if (!ScriptEngine::Get().LoadUserAssembly(FS::GetExecutableDirectory() / "EppoTesting.Scripts.dll"))
                        return false;
                    return ScriptEngine::Get().IsValidScriptClass(kUserClass);
                }
                catch (...)
                {
                    return false;
                }
            }();
            return ready;
        }

        auto FindClass(const std::string& fullName) -> const ScriptClass*
        {
            const auto& classes = ScriptEngine::Get().GetClasses();
            const auto it = std::ranges::find_if(
                classes,
                [&](const ScriptClass& c)
                {
                    return c.GetFullName() == fullName;
                }
            );
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

        // A dynamic, scripted rigid body for the runtime-lifecycle tests. The
        // collider gives it mass, so the impulse LifecycleScript applies from
        // OnCreate produces a readable velocity.
        auto MakeLifecycleBody(const Ref<Scene>& scene) -> Entity
        {
            Entity entity = scene->CreateEntity("LifecycleOwner");
            entity.AddComponent<ScriptComponent>(std::string(kLifecycleClass));
            entity.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Dynamic;
            entity.AddComponent<SphereColliderComponent>();
            return entity;
        }

        auto HasEntityNamed(const Ref<Scene>& scene, const std::string& name) -> bool
        {
            bool found = false;
            scene->ForEachEntity(
                [&](Entity e)
                {
                    found = found || e.GetName() == name;
                }
            );
            return found;
        }
    }

    // --- Class metadata: the runtime comes up and reflects the user class. ---

    TEST(ScriptEngine_IsUserAssemblyValid_BeforeInitialization_IsFalse)
    {
        CHECK_EQUAL(false, ScriptEngine::IsUserAssemblyValid());
    }

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
        const auto speed = std::ranges::find_if(
            fields,
            [](const ScriptField& f)
            {
                return f.Name == "Speed";
            }
        );
        REQUIRE CHECK(speed != fields.end());
        CHECK(speed->Type == ScriptFieldType::Float);
    }

    TEST(ScriptEngine_PublicFieldInitializers_AreReadWithoutCreatingOverrides)
    {
        REQUIRE CHECK(EnsureRuntime());
        auto& engine = ScriptEngine::Get();
        const int32_t classIndex = engine.FindClassIndex(kUserClass);
        REQUIRE CHECK(classIndex >= 0);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const Eppo::UUID entityId;
        CHECK(engine.TryGetFieldMap(entityId) == nullptr);

        CHECK_CLOSE(2.5f, engine.GetFieldValueOrDefault(entityId, classIndex, FieldIndex(*c, "Speed")).Get<float>(), 1e-5f);
        CHECK_EQUAL(7, engine.GetFieldValueOrDefault(entityId, classIndex, FieldIndex(*c, "Count")).Get<int32_t>());
        CHECK_EQUAL(true, engine.GetFieldValueOrDefault(entityId, classIndex, FieldIndex(*c, "Enabled")).Get<bool>());
        CHECK_CLOSE(1.5, engine.GetFieldValueOrDefault(entityId, classIndex, FieldIndex(*c, "Ratio")).Get<double>(), 1e-9);

        CHECK(engine.TryGetFieldMap(entityId) == nullptr);
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
        engine.OnDestroyEntity(entity); // no live instance: safe no-op
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

    // --- Runtime lifecycle: Scene publishes the scripting scene context, so
    // OnCreate and OnDestroy both run against a resolvable scene and a live
    // physics world. Asserted through native scene/physics state, since the
    // managed instance is destroyed as OnDestroy returns. ---

    TEST(Scene_OnRuntimeStart_RunsOnCreateWithSceneAndPhysicsContext)
    {
        REQUIRE CHECK(EnsureRuntime());

        // Scene must publish the context itself; don't inherit one from a prior test.
        ScriptEngine::Get().SetSceneContext(nullptr);

        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = MakeLifecycleBody(scene);

        scene->OnRuntimeStart();

        // Component write from OnCreate landed on the native transform.
        CHECK_VEC3_CLOSE(glm::vec3(1.0f, 2.0f, 3.0f), entity.GetComponent<TransformComponent>().Translation, 1e-5f);

        // Scene op from OnCreate produced a real entity.
        CHECK(HasEntityNamed(scene, "CreatedFromOnCreate"));

        // Physics op from OnCreate reached the live body.
        REQUIRE CHECK(scene->GetPhysicsWorld() != nullptr);
        CHECK(scene->GetPhysicsWorld()->GetLinearVelocity(entity.GetUUID()).y > 0.0f);

        scene->OnRuntimeStop();
    }

    TEST(Scene_OnRuntimeStop_RunsOnDestroyWithSceneAndPhysicsContext)
    {
        REQUIRE CHECK(EnsureRuntime());

        ScriptEngine::Get().SetSceneContext(nullptr);

        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = MakeLifecycleBody(scene);

        scene->OnRuntimeStart();
        scene->OnRuntimeStop();

        // Scene op from OnDestroy produced a real entity.
        CHECK(HasEntityNamed(scene, "CreatedFromOnDestroy"));

        // OnDestroy overwrote the translation with the velocity it read back, so a
        // non-zero y proves the physics world was still live. Zero would mean the
        // scene resolved but physics was already released; OnCreate's (1,2,3) that
        // neither resolved.
        CHECK(entity.GetComponent<TransformComponent>().Translation.y > 0.0f);
    }

    // A successful explicit load is what makes the assembly valid for play mode.
    TEST(ScriptEngine_IsUserAssemblyValid_AfterExplicitLoad_IsTrue)
    {
        REQUIRE CHECK(EnsureRuntime());

        CHECK_EQUAL(true, ScriptEngine::IsUserAssemblyValid());
    }

    // Hot reload polls every frame, including while no project is open.
    TEST(ScriptEngine_OnUpdate_WithoutAnActiveProject_DoesNothing)
    {
        REQUIRE CHECK(EnsureRuntime());

        ScriptEngine::Get().VerifyRuntime();

        CHECK_EQUAL(true, ScriptEngine::IsUserAssemblyValid());
    }

    TEST(ScriptEngine_ReloadProjectAssembly_WithoutAnActiveProject_Fails)
    {
        REQUIRE CHECK(EnsureRuntime());

        CHECK_EQUAL(false, ScriptEngine::Get().ReloadProjectAssembly());
    }

    // Editor-managed projects live under the root directory, so a script build that
    // writes above them compiles an empty assembly: the .NET SDK excludes everything
    // under OutputPath from the default compile glob, and reports success anyway.
    TEST(ScriptEngine_ReloadProjectAssembly_ForProjectUnderTheProjectsDirectory_DiscoversScriptClasses)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Testing::TempDir projectDirectory(Project::GetProjectsDirectory());
        const auto scriptsDirectory = projectDirectory.Path() / "Scripts";
        std::filesystem::create_directories(scriptsDirectory / "Source");

        // Mirrors the new-project template: default compile items, core resolved
        // through the CoreManagedDll property the engine passes.
        REQUIRE CHECK(
            FS::WriteText(
                scriptsDirectory / "ScriptProbe.csproj", R"(<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework>
    <Nullable>enable</Nullable>
  </PropertyGroup>
  <ItemGroup>
    <Reference Include="EppoScriptCore">
      <HintPath>$(CoreManagedDll)</HintPath>
      <Private>false</Private>
    </Reference>
  </ItemGroup>
</Project>
)",
                true
            )
        );

        REQUIRE CHECK(
            FS::WriteText(
                scriptsDirectory / "Source" / "ProbeScript.cs", R"(using EppoScriptCore.Scene;

namespace EppoTesting
{
    public class ProbeScript : Entity
    {
    }
}
)",
                true
            )
        );

        Project::New(
            ProjectSpecification{
                .Name = "ScriptProbe",
                .ProjectDirectory = projectDirectory.Path(),
            }
        );

        const bool reloaded = ScriptEngine::Get().ReloadProjectAssembly();
        const bool discovered = ScriptEngine::Get().IsValidScriptClass("EppoTesting.ProbeScript");

        // The suite shares one runtime, so hand the harness assembly back before
        // asserting — a failure here must not take every later test with it.
        Project::SetActive(nullptr);
        ScriptEngine::Get().LoadUserAssembly(FS::GetExecutableDirectory() / "EppoTesting.Scripts.dll");

        CHECK_EQUAL(true, reloaded);
        CHECK_EQUAL(true, discovered);
    }

    // Covers the addition case only, and passes with or without the snapshot: entt's
    // storage is paged and views iterate in reverse, so appends fall outside the walk.
    // Component removal is the genuinely unsafe mutation and is not covered here.
    TEST(Scene_ScriptSpawnsScriptedEntitiesFromLifecycleHooks_Succeeds)
    {
        REQUIRE CHECK(EnsureRuntime());

        ScriptEngine::Get().SetSceneContext(nullptr);

        const Ref<Scene> scene = CreateRef<Scene>();
        Entity entity = scene->CreateEntity("Spawner");
        entity.AddComponent<ScriptComponent>(std::string("EppoTesting.SpawningScript"));

        scene->OnRuntimeStart();

        size_t spawnedFromCreate = 0;
        scene->ForEachEntity(
            [&](Entity e)
            {
                if (e.GetName().starts_with("SpawnedFromCreate"))
                    spawnedFromCreate++;
            }
        );
        CHECK_EQUAL(size_t(8), spawnedFromCreate);

        scene->OnRuntimeStop();

        size_t spawnedFromDestroy = 0;
        scene->ForEachEntity(
            [&](Entity e)
            {
                if (e.GetName().starts_with("SpawnedFromDestroy"))
                    spawnedFromDestroy++;
            }
        );
        CHECK_EQUAL(size_t(8), spawnedFromDestroy);
    }

    TEST(Scene_OnRuntimeStop_ClearsSceneContext)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        MakeLifecycleBody(scene);

        scene->OnRuntimeStart();
        CHECK(ScriptEngine::Get().GetSceneContext() == scene);

        scene->OnRuntimeStop();
        CHECK(ScriptEngine::Get().GetSceneContext() == nullptr);
    }

    // --- Internal calls (ScriptGlue): each test invokes the 1:1 harness forwarder for one
    // internal call and asserts on native state / the returned value. ---

    // --- Writable mesh handles: a script can make a spawned entity visible. ---

    TEST(MeshComponent_SetMeshHandle_AssignsPrimitiveHandle)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        entity.AddComponent<MeshComponent>();

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* setHandle = c->GetMethod("MeshComponent_SetMeshHandle");
        REQUIRE CHECK(setHandle != nullptr);

        const uint64_t cube = static_cast<uint64_t>(MeshPrimitiveType::Cube);
        c->InvokeMethod(entity, *setHandle, &cube, nullptr);

        CHECK(entity.GetComponent<MeshComponent>().MeshHandle == AssetHandle(cube));

        engine.OnDestroyEntity(entity);
    }

    // The typed enum is the API scripts are meant to use; it must land on the same
    // reserved handle the asset manager generates primitives from.
    TEST(MeshComponent_SetPrimitive_MapsEnumToReservedHandle)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        entity.AddComponent<MeshComponent>();

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* setPrimitive = c->GetMethod("MeshComponent_SetPrimitiveCube");
        REQUIRE CHECK(setPrimitive != nullptr);

        c->InvokeMethod(entity, *setPrimitive, nullptr, nullptr);

        const AssetHandle assigned = entity.GetComponent<MeshComponent>().MeshHandle;
        CHECK(assigned == AssetHandle(static_cast<uint64_t>(MeshPrimitiveType::Cube)));

        engine.OnDestroyEntity(entity);
    }

    // Without an active project an unregistered handle can't be verified as a mesh,
    // so it must be refused rather than assigned on trust.
    TEST(MeshComponent_SetMeshHandle_UnknownHandle_LeavesComponentUnchanged)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        entity.AddComponent<MeshComponent>();

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* setHandle = c->GetMethod("MeshComponent_SetMeshHandle");
        REQUIRE CHECK(setHandle != nullptr);

        // Seed a known-good value so a rejected assignment is distinguishable.
        const uint64_t sphere = static_cast<uint64_t>(MeshPrimitiveType::Sphere);
        c->InvokeMethod(entity, *setHandle, &sphere, nullptr);

        const uint64_t bogus = 0xDEADBEEFull;
        c->InvokeMethod(entity, *setHandle, &bogus, nullptr);

        CHECK(entity.GetComponent<MeshComponent>().MeshHandle == AssetHandle(sphere));

        engine.OnDestroyEntity(entity);
    }

    TEST(MeshComponent_SetMeshHandle_ZeroClearsAssignment)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        entity.AddComponent<MeshComponent>();

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* setHandle = c->GetMethod("MeshComponent_SetMeshHandle");
        REQUIRE CHECK(setHandle != nullptr);

        const uint64_t cube = static_cast<uint64_t>(MeshPrimitiveType::Cube);
        c->InvokeMethod(entity, *setHandle, &cube, nullptr);

        const uint64_t none = 0;
        c->InvokeMethod(entity, *setHandle, &none, nullptr);

        CHECK(entity.GetComponent<MeshComponent>().MeshHandle == AssetHandle(0));

        engine.OnDestroyEntity(entity);
    }

    // The end-to-end case the feature exists for: spawn an entity from a script and
    // give it a visible mesh, with no asset handle known to the script author.
    TEST(Scene_CreateEntity_WithPrimitiveMesh_IsVisibleFromScript)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* spawn = c->GetMethod("Scene_CreateEntityWithPrimitive");
        REQUIRE CHECK(spawn != nullptr);

        uint64_t spawnedId = 0;
        c->InvokeMethod(entity, *spawn, nullptr, &spawnedId);

        Entity spawned = scene->GetEntityByUUID(Eppo::UUID(spawnedId));
        REQUIRE CHECK(static_cast<bool>(spawned));
        REQUIRE CHECK(spawned.HasComponent<MeshComponent>());
        CHECK(spawned.GetComponent<MeshComponent>().MeshHandle == AssetHandle(static_cast<uint64_t>(MeshPrimitiveType::Sphere)));

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

    TEST(TransformComponent_Rotation_RoundTripsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& tc = entity.GetComponent<TransformComponent>();
        tc.Rotation = glm::vec3(0.1f, 0.2f, 0.3f);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        glm::vec3 rotation{};
        const ScriptMethod* getRotation = c->GetMethod("TransformComponent_GetRotation");
        REQUIRE CHECK(getRotation != nullptr);
        c->InvokeMethod(entity, *getRotation, nullptr, &rotation);
        CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), rotation, 1e-5f);

        const glm::vec3 next(0.4f, 0.5f, 0.6f);
        const ScriptMethod* setRotation = c->GetMethod("TransformComponent_SetRotation");
        REQUIRE CHECK(setRotation != nullptr);
        c->InvokeMethod(entity, *setRotation, &next, nullptr);
        CHECK_VEC3_CLOSE(next, tc.Rotation, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(TransformComponent_Scale_RoundTripsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& tc = entity.GetComponent<TransformComponent>();
        tc.Scale = glm::vec3(2.0f, 3.0f, 4.0f);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        glm::vec3 scale{};
        const ScriptMethod* getScale = c->GetMethod("TransformComponent_GetScale");
        REQUIRE CHECK(getScale != nullptr);
        c->InvokeMethod(entity, *getScale, nullptr, &scale);
        CHECK_VEC3_CLOSE(glm::vec3(2.0f, 3.0f, 4.0f), scale, 1e-5f);

        const glm::vec3 next(0.5f, 0.25f, 0.125f);
        const ScriptMethod* setScale = c->GetMethod("TransformComponent_SetScale");
        REQUIRE CHECK(setScale != nullptr);
        c->InvokeMethod(entity, *setScale, &next, nullptr);
        CHECK_VEC3_CLOSE(next, tc.Scale, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(RigidBodyComponent_GravityScale_RoundTripsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& rb = entity.AddComponent<RigidBodyComponent>();
        rb.GravityScale = 2.5f;

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        float gravityScale = 0.0f;
        const ScriptMethod* getGravity = c->GetMethod("RigidBodyComponent_GetGravityScale");
        REQUIRE CHECK(getGravity != nullptr);
        c->InvokeMethod(entity, *getGravity, nullptr, &gravityScale);
        CHECK_CLOSE(2.5f, gravityScale, 1e-5f);

        const float next = 0.75f;
        const ScriptMethod* setGravity = c->GetMethod("RigidBodyComponent_SetGravityScale");
        REQUIRE CHECK(setGravity != nullptr);
        c->InvokeMethod(entity, *setGravity, &next, nullptr);
        CHECK_CLOSE(0.75f, rb.GravityScale, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(RigidBodyComponent_LinearDamping_RoundTripsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& rb = entity.AddComponent<RigidBodyComponent>();
        rb.LinearDamping = 0.3f;

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        float linearDamping = 0.0f;
        const ScriptMethod* getDamping = c->GetMethod("RigidBodyComponent_GetLinearDamping");
        REQUIRE CHECK(getDamping != nullptr);
        c->InvokeMethod(entity, *getDamping, nullptr, &linearDamping);
        CHECK_CLOSE(0.3f, linearDamping, 1e-5f);

        const float next = 0.9f;
        const ScriptMethod* setDamping = c->GetMethod("RigidBodyComponent_SetLinearDamping");
        REQUIRE CHECK(setDamping != nullptr);
        c->InvokeMethod(entity, *setDamping, &next, nullptr);
        CHECK_CLOSE(0.9f, rb.LinearDamping, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(RigidBodyComponent_AngularDamping_RoundTripsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& rb = entity.AddComponent<RigidBodyComponent>();
        rb.AngularDamping = 0.2f;

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        float angularDamping = 0.0f;
        const ScriptMethod* getDamping = c->GetMethod("RigidBodyComponent_GetAngularDamping");
        REQUIRE CHECK(getDamping != nullptr);
        c->InvokeMethod(entity, *getDamping, nullptr, &angularDamping);
        CHECK_CLOSE(0.2f, angularDamping, 1e-5f);

        const float next = 0.6f;
        const ScriptMethod* setDamping = c->GetMethod("RigidBodyComponent_SetAngularDamping");
        REQUIRE CHECK(setDamping != nullptr);
        c->InvokeMethod(entity, *setDamping, &next, nullptr);
        CHECK_CLOSE(0.6f, rb.AngularDamping, 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(RigidBodyComponent_MotionLocks_RoundTripSceneValues)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& rb = entity.AddComponent<RigidBodyComponent>();
        rb.LockLinearZ = true;
        rb.LockAngularX = true;

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        const std::pair<const char*, bool> getters[] = {
            { "RigidBodyComponent_GetLockLinearX",  false },
            { "RigidBodyComponent_GetLockLinearY",  false },
            { "RigidBodyComponent_GetLockLinearZ",  true  },
            { "RigidBodyComponent_GetLockAngularX", true  },
            { "RigidBodyComponent_GetLockAngularY", false },
            { "RigidBodyComponent_GetLockAngularZ", false },
        };
        for (const auto& [name, expected] : getters)
        {
            const ScriptMethod* getter = c->GetMethod(name);
            REQUIRE CHECK(getter != nullptr);
            bool got = !expected;
            c->InvokeMethod(entity, *getter, nullptr, &got);
            CHECK_EQUAL(expected, got);
        }

        const char* setters[] = {
            "RigidBodyComponent_SetLockLinearX",  "RigidBodyComponent_SetLockLinearY",  "RigidBodyComponent_SetLockLinearZ",
            "RigidBodyComponent_SetLockAngularX", "RigidBodyComponent_SetLockAngularY", "RigidBodyComponent_SetLockAngularZ",
        };
        for (const char* name : setters)
        {
            const ScriptMethod* setter = c->GetMethod(name);
            REQUIRE CHECK(setter != nullptr);
            bool next = true;
            c->InvokeMethod(entity, *setter, &next, nullptr);
        }

        CHECK(rb.LockLinearX && rb.LockLinearY && rb.LockLinearZ);
        CHECK(rb.LockAngularX && rb.LockAngularY && rb.LockAngularZ);

        engine.OnDestroyEntity(entity);
    }

    TEST(Entity_SetName_MutatesScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        // The harness forwarder writes this exact string via Entity_SetName.
        const ScriptMethod* setName = c->GetMethod("Entity_SetName");
        REQUIRE CHECK(setName != nullptr);

        c->InvokeMethod(entity, *setName, nullptr, nullptr);
        CHECK_EQUAL(std::string("RenamedFromScript"), entity.GetName());

        engine.OnDestroyEntity(entity);
    }

    TEST(RelationshipComponent_GetChildren_ReturnsSceneChildren)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity parent = MakeContextEntity(scene); // scripted invoker
        Entity first = scene->CreateEntity("First");
        Entity second = scene->CreateEntity("Second");
        scene->SetParent(first, parent);
        scene->SetParent(second, parent);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        int32_t count = 0;
        const ScriptMethod* getCount = c->GetMethod("RelationshipComponent_GetChildCount");
        REQUIRE CHECK(getCount != nullptr);
        c->InvokeMethod(parent, *getCount, nullptr, &count);
        CHECK_EQUAL(2, count);

        const ScriptMethod* getChild = c->GetMethod("RelationshipComponent_GetChild");
        REQUIRE CHECK(getChild != nullptr);
        const int32_t zero = 0;
        const int32_t one = 1;
        uint64_t child0 = 0;
        uint64_t child1 = 0;
        c->InvokeMethod(parent, *getChild, &zero, &child0);
        c->InvokeMethod(parent, *getChild, &one, &child1);

        const uint64_t firstId = static_cast<uint64_t>(first.GetUUID());
        const uint64_t secondId = static_cast<uint64_t>(second.GetUUID());
        CHECK((child0 == firstId && child1 == secondId) || (child0 == secondId && child1 == firstId));

        engine.OnDestroyEntity(parent);
    }

    TEST(CameraComponent_VerticalFov_RoundTripsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& camera = entity.AddComponent<CameraComponent>();
        camera.Camera.SetPerspectiveVerticalFov(60.0f);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        float fov = 0.0f;
        const ScriptMethod* getFov = c->GetMethod("CameraComponent_GetVerticalFov");
        REQUIRE CHECK(getFov != nullptr);
        c->InvokeMethod(entity, *getFov, nullptr, &fov);
        CHECK_CLOSE(60.0f, fov, 1e-4f);

        const float next = 75.0f;
        const ScriptMethod* setFov = c->GetMethod("CameraComponent_SetVerticalFov");
        REQUIRE CHECK(setFov != nullptr);
        c->InvokeMethod(entity, *setFov, &next, nullptr);
        CHECK_CLOSE(75.0f, camera.Camera.GetPerspectiveVerticalFov(), 1e-4f);

        engine.OnDestroyEntity(entity);
    }

    TEST(CameraComponent_NearClip_RoundTripsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& camera = entity.AddComponent<CameraComponent>();
        camera.Camera.SetPerspectiveNearClip(0.25f);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        float nearClip = 0.0f;
        const ScriptMethod* getNear = c->GetMethod("CameraComponent_GetNearClip");
        REQUIRE CHECK(getNear != nullptr);
        c->InvokeMethod(entity, *getNear, nullptr, &nearClip);
        CHECK_CLOSE(0.25f, nearClip, 1e-5f);

        const float next = 0.5f;
        const ScriptMethod* setNear = c->GetMethod("CameraComponent_SetNearClip");
        REQUIRE CHECK(setNear != nullptr);
        c->InvokeMethod(entity, *setNear, &next, nullptr);
        CHECK_CLOSE(0.5f, camera.Camera.GetPerspectiveNearClip(), 1e-5f);

        engine.OnDestroyEntity(entity);
    }

    TEST(CameraComponent_FarClip_RoundTripsSceneValue)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        auto& camera = entity.AddComponent<CameraComponent>();
        camera.Camera.SetPerspectiveFarClip(500.0f);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);

        float farClip = 0.0f;
        const ScriptMethod* getFar = c->GetMethod("CameraComponent_GetFarClip");
        REQUIRE CHECK(getFar != nullptr);
        c->InvokeMethod(entity, *getFar, nullptr, &farClip);
        CHECK_CLOSE(500.0f, farClip, 1e-3f);

        const float next = 250.0f;
        const ScriptMethod* setFar = c->GetMethod("CameraComponent_SetFarClip");
        REQUIRE CHECK(setFar != nullptr);
        c->InvokeMethod(entity, *setFar, &next, nullptr);
        CHECK_CLOSE(250.0f, camera.Camera.GetPerspectiveFarClip(), 1e-3f);

        engine.OnDestroyEntity(entity);
    }

    TEST(Scene_CreateEntity_AddsEntityToScene)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity invoker = MakeContextEntity(scene);

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* create = c->GetMethod("Scene_CreateEntity");
        REQUIRE CHECK(create != nullptr);

        uint64_t spawnedId = 0;
        c->InvokeMethod(invoker, *create, nullptr, &spawnedId);
        CHECK(spawnedId != 0);

        Entity spawned = scene->GetEntityByUUID(Eppo::UUID(spawnedId));
        REQUIRE CHECK(static_cast<bool>(spawned));
        CHECK_EQUAL(std::string("Spawned"), spawned.GetName());
        CHECK(spawnedId != static_cast<uint64_t>(invoker.GetUUID()));

        engine.OnDestroyEntity(invoker);
    }

    TEST(Scene_DestroyEntity_DefersUntilFrameEnd)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity invoker = MakeContextEntity(scene);
        Entity doomed = scene->CreateEntity("Doomed");
        const uint64_t doomedId = static_cast<uint64_t>(doomed.GetUUID());

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const ScriptMethod* destroy = c->GetMethod("Scene_DestroyEntity");
        REQUIRE CHECK(destroy != nullptr);

        c->InvokeMethod(invoker, *destroy, &doomedId, nullptr);
        // Deferred: the entity is still present until the frame's destroy queue drains.
        CHECK(static_cast<bool>(scene->GetEntityByUUID(Eppo::UUID(doomedId))));

        scene->OnUpdateRuntime(0.0f); // drains the queue after the script update loop
        CHECK(!static_cast<bool>(scene->GetEntityByUUID(Eppo::UUID(doomedId))));

        engine.OnDestroyEntity(invoker);
    }

    // Destroying a scripted entity must run its managed OnDestroy and unregister the
    // live instance, not leak it until engine shutdown.
    TEST(Scene_DestroyEntity_TearsDownScriptInstance)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity entity = MakeContextEntity(scene);
        const Eppo::UUID id = entity.GetUUID();
        REQUIRE CHECK(engine.GetEntityInstance(id) != nullptr);

        scene->DestroyEntity(entity);
        CHECK(engine.GetEntityInstance(id) == nullptr);
    }

    // A scripted entity destroying itself from inside OnUpdate must not corrupt the
    // ScriptComponent view being iterated: the destroy is deferred to the frame's
    // drain, the entity is gone afterward, and other scripts still run.
    TEST(Scene_DestroyEntity_SelfDuringUpdate_IsSafe)
    {
        REQUIRE CHECK(EnsureRuntime());

        const Ref<Scene> scene = CreateRef<Scene>();
        auto& engine = ScriptEngine::Get();
        Entity selfDestruct = MakeContextEntity(scene);
        Entity survivor = MakeContextEntity(scene); // a second entity in the script view

        const ScriptClass* c = FindClass(kUserClass);
        REQUIRE CHECK(c != nullptr);
        const int32_t flagIndex = FieldIndex(*c, "DestroySelfOnUpdate");
        REQUIRE CHECK(flagIndex >= 0);

        ScriptInstance* instance = engine.GetEntityInstance(selfDestruct.GetUUID());
        REQUIRE CHECK(instance != nullptr);
        bool destroySelf = true;
        instance->SetFieldValue(flagIndex, &destroySelf);

        const Eppo::UUID selfId = selfDestruct.GetUUID();
        const Eppo::UUID survivorId = survivor.GetUUID();

        scene->OnUpdateRuntime(0.016f); // self-destruct queued mid-loop, drained after

        CHECK(!static_cast<bool>(scene->GetEntityByUUID(selfId))); // self-destructed
        CHECK(static_cast<bool>(scene->GetEntityByUUID(survivorId))); // survivor intact
        CHECK(engine.GetEntityInstance(selfId) == nullptr); // instance torn down

        engine.OnDestroyEntity(survivor);
    }
}
