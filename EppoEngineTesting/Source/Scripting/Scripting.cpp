#include "TestSupport/EppoTest.h"
#include "TestSupport/GlmCheck.h"
#include "TestSupport/TempDir.h"
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

TEST(Scripting, ScriptEngine_IsUserAssemblyValid_BeforeInitialization_IsFalse)
{
    EXPECT_EQ(false, ScriptEngine::IsUserAssemblyValid());
}

TEST(Scripting, ScriptEngine_Init_DiscoversClasses)
{
    EP_REQUIRE(EnsureRuntime());
    EXPECT_TRUE(!ScriptEngine::Get().GetClasses().empty());
}

TEST(Scripting, ScriptEngine_UserClass_IsDiscovered)
{
    EP_REQUIRE(EnsureRuntime());
    EXPECT_TRUE(ScriptEngine::Get().IsValidScriptClass(kUserClass));
}

TEST(Scripting, ScriptEngine_EntityBaseClass_NotRegistered)
{
    EP_REQUIRE(EnsureRuntime());
    EXPECT_TRUE(!ScriptEngine::Get().IsValidScriptClass("EppoScriptCore.Scene.Entity"));
    EXPECT_EQ(-1, ScriptEngine::Get().FindClassIndex("EppoScriptCore.Scene.Entity"));
}

TEST(Scripting, ScriptClass_PublicFields_AreReflected)
{
    EP_REQUIRE(EnsureRuntime());
    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    const auto& fields = c->GetFields();
    const auto speed = std::ranges::find_if(
        fields,
        [](const ScriptField& f)
        {
            return f.Name == "Speed";
        }
    );
    EP_REQUIRE(speed != fields.end());
    EXPECT_TRUE(speed->Type == ScriptFieldType::Float);
}

TEST(Scripting, ScriptEngine_PublicFieldInitializers_AreReadWithoutCreatingOverrides)
{
    EP_REQUIRE(EnsureRuntime());
    auto& engine = ScriptEngine::Get();
    const int32_t classIndex = engine.FindClassIndex(kUserClass);
    EP_REQUIRE(classIndex >= 0);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const Eppo::UUID entityId;
    EXPECT_TRUE(engine.TryGetFieldMap(entityId) == nullptr);

    EXPECT_NEAR(2.5f, engine.GetFieldValueOrDefault(entityId, classIndex, FieldIndex(*c, "Speed")).Get<float>(), 1e-5f);
    EXPECT_EQ(7, engine.GetFieldValueOrDefault(entityId, classIndex, FieldIndex(*c, "Count")).Get<int32_t>());
    EXPECT_EQ(true, engine.GetFieldValueOrDefault(entityId, classIndex, FieldIndex(*c, "Enabled")).Get<bool>());
    EXPECT_NEAR(1.5, engine.GetFieldValueOrDefault(entityId, classIndex, FieldIndex(*c, "Ratio")).Get<double>(), 1e-9);

    EXPECT_TRUE(engine.TryGetFieldMap(entityId) == nullptr);
}

TEST(Scripting, ScriptClass_PublicMethods_AreReflected)
{
    EP_REQUIRE(EnsureRuntime());
    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    const ScriptMethod* add = c->GetMethod("Add");
    EP_REQUIRE(add != nullptr);
    EXPECT_EQ("Add", add->Name);
    EXPECT_TRUE(add->Index >= 0);
    EXPECT_TRUE(c->GetMethod("DoesNotExist") == nullptr);
}

// An entity referencing an unknown class must not instantiate, and the
// lifecycle calls around it must stay safe no-ops.
TEST(Scripting, ScriptEngine_UnknownClass_ProducesNoInstance)
{
    EP_REQUIRE(EnsureRuntime());

    auto& engine = ScriptEngine::Get();
    EXPECT_TRUE(!engine.IsValidScriptClass("EppoTesting.NoSuchClass"));
    EXPECT_EQ(-1, engine.FindClassIndex("EppoTesting.NoSuchClass"));

    const Ref<Scene> scene = CreateRef<Scene>();
    Entity entity = scene->CreateEntity("Bad");
    entity.AddComponent<ScriptComponent>(std::string("EppoTesting.NoSuchClass"));

    engine.OnCreateEntity(entity);
    EXPECT_TRUE(engine.GetEntityInstance(entity.GetUUID()) == nullptr);

    engine.OnUpdateEntity(entity, 0.016f); // no live instance: safe no-op
    engine.OnDestroyEntity(entity); // no live instance: safe no-op
    EXPECT_TRUE(true);
}

// --- Instance lifecycle (ScriptGlue create/update/destroy). ---

// OnCreate sets Created=1 and OnUpdate accumulates deltaTime on the harness;
// reading those fields back proves both lifecycle calls ran managed code.
TEST(Scripting, ScriptEngine_CreateAndUpdate_RunManagedCode)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeLiveEntity(scene);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
    EP_REQUIRE(instance != nullptr);

    int32_t created = 0;
    instance->GetFieldValue(FieldIndex(*c, "Created"), &created);
    EXPECT_EQ(1, created);

    engine.OnUpdateEntity(entity, 0.5f);
    engine.OnUpdateEntity(entity, 0.25f);

    float accumulated = 0.0f;
    instance->GetFieldValue(FieldIndex(*c, "Accumulated"), &accumulated);
    EXPECT_NEAR(0.75f, accumulated, 1e-5f);

    engine.OnDestroyEntity(entity);
}

// Stopping play (OnDestroyEntity) must remove the entity from the live
// registry so GetEntityInstance no longer returns a handle.
TEST(Scripting, ScriptEngine_DestroyEntity_UnregistersInstance)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeLiveEntity(scene);
    EXPECT_TRUE(engine.GetEntityInstance(entity.GetUUID()) != nullptr);

    engine.OnDestroyEntity(entity);
    EXPECT_TRUE(engine.GetEntityInstance(entity.GetUUID()) == nullptr);
}

// --- Method invocation marshalling (ScriptGlue InvokeMethod). ---

TEST(Scripting, ScriptClass_InvokeMethod_MarshalsArgsAndReturn)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    Entity entity = scene->CreateEntity("Scripted");
    entity.AddComponent<ScriptComponent>(std::string(kUserClass));

    auto& engine = ScriptEngine::Get();
    engine.OnCreateEntity(entity);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* add = c->GetMethod("Add");
    EP_REQUIRE(add != nullptr);

    const int32_t args[2] = { 20, 22 }; // packed back-to-back
    int32_t result = 0;
    c->InvokeMethod(entity, *add, args, &result);
    EXPECT_EQ(42, result);

    engine.OnDestroyEntity(entity);
}

// A managed exception thrown from user code must not escape the
// UnmanagedCallersOnly boundary and fail-fast the host process.
TEST(Scripting, ScriptClass_ManagedException_DoesNotCrashHost)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    Entity entity = scene->CreateEntity("Scripted");
    entity.AddComponent<ScriptComponent>(std::string(kUserClass));

    auto& engine = ScriptEngine::Get();
    engine.OnCreateEntity(entity);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* throws = c->GetMethod("Throws");
    EP_REQUIRE(throws != nullptr);

    int32_t result = -1;
    c->InvokeMethod(entity, *throws, nullptr, &result);

    // Reaching here proves the managed exception did not fail-fast the host process.
    EXPECT_TRUE(true);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, Entity_NullEquality_IsSafe)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    Entity entity = scene->CreateEntity("Scripted");
    entity.AddComponent<ScriptComponent>(std::string(kUserClass));

    auto& engine = ScriptEngine::Get();
    engine.OnCreateEntity(entity);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* nullSafe = c->GetMethod("NullEqualityIsSafe");
    EP_REQUIRE(nullSafe != nullptr);

    bool safe = false;
    c->InvokeMethod(entity, *nullSafe, nullptr, &safe);
    EXPECT_EQ(true, safe);

    engine.OnDestroyEntity(entity);
}

// --- Field marshalling (ScriptGlue Get/SetFieldValue). ---

TEST(Scripting, ScriptInstance_IntField_MarshalsValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeLiveEntity(scene);

    ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
    EP_REQUIRE(instance != nullptr);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const int32_t countIndex = FieldIndex(*c, "Count");
    EP_REQUIRE(countIndex >= 0);

    int32_t value = 99;
    instance->SetFieldValue(countIndex, &value);
    int32_t readBack = 0;
    instance->GetFieldValue(countIndex, &readBack);
    EXPECT_EQ(99, readBack);

    engine.OnDestroyEntity(entity);
}

// A Vector3 field must be reported with its real type (not None) and marshal
// its 12 bytes both ways — regression cover for ManagedTypeToFieldType.
TEST(Scripting, ScriptInstance_Vector3Field_IsTypedAndMarshals)
{
    EP_REQUIRE(EnsureRuntime());

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const int32_t posIndex = FieldIndex(*c, "Position");
    EP_REQUIRE(posIndex >= 0);
    EXPECT_TRUE(c->GetFields()[posIndex].Type == ScriptFieldType::Vector3);

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeLiveEntity(scene);
    ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
    EP_REQUIRE(instance != nullptr);

    const float in[3] = { 1.5f, -2.0f, 3.25f };
    instance->SetFieldValue(posIndex, in);
    float out[3] = { 0.0f, 0.0f, 0.0f };
    instance->GetFieldValue(posIndex, out);
    EXPECT_NEAR(1.5f, out[0], 1e-5f);
    EXPECT_NEAR(-2.0f, out[1], 1e-5f);
    EXPECT_NEAR(3.25f, out[2], 1e-5f);

    engine.OnDestroyEntity(entity);
}

// An Entity field (a managed reference type) must be typed as Entity and
// marshal as its 8-byte id both ways.
TEST(Scripting, ScriptInstance_EntityField_IsTypedAndMarshals)
{
    EP_REQUIRE(EnsureRuntime());

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const int32_t targetIndex = FieldIndex(*c, "Target");
    EP_REQUIRE(targetIndex >= 0);
    EXPECT_TRUE(c->GetFields()[targetIndex].Type == ScriptFieldType::Entity);

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeLiveEntity(scene);
    ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
    EP_REQUIRE(instance != nullptr);

    const uint64_t id = 0xABCDEF0123456789ull;
    instance->SetFieldValue(targetIndex, &id);
    uint64_t back = 0;
    instance->GetFieldValue(targetIndex, &back);
    EXPECT_TRUE(back == id);

    engine.OnDestroyEntity(entity);
}

// bool and double fields round-trip through the marshalling layer intact.
TEST(Scripting, ScriptInstance_BoolAndDoubleFields_Marshal)
{
    EP_REQUIRE(EnsureRuntime());

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const int32_t enabledIndex = FieldIndex(*c, "Enabled");
    const int32_t ratioIndex = FieldIndex(*c, "Ratio");
    EP_REQUIRE(enabledIndex >= 0);
    EP_REQUIRE(ratioIndex >= 0);
    EXPECT_TRUE(c->GetFields()[enabledIndex].Type == ScriptFieldType::Bool);
    EXPECT_TRUE(c->GetFields()[ratioIndex].Type == ScriptFieldType::Double);

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeLiveEntity(scene);
    ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
    EP_REQUIRE(instance != nullptr);

    bool enabled = false;
    instance->SetFieldValue(enabledIndex, &enabled);
    bool enabledBack = true;
    instance->GetFieldValue(enabledIndex, &enabledBack);
    EXPECT_EQ(false, enabledBack);

    double ratio = 2.75;
    instance->SetFieldValue(ratioIndex, &ratio);
    double ratioBack = 0.0;
    instance->GetFieldValue(ratioIndex, &ratioBack);
    EXPECT_NEAR(2.75, ratioBack, 1e-9);

    engine.OnDestroyEntity(entity);
}

// Editor-time field values in the side table must be pushed into the fresh
// managed instance when the entity's script is created on play.
TEST(Scripting, ScriptEngine_EditorFieldValues_PushedOnCreate)
{
    EP_REQUIRE(EnsureRuntime());

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
    EP_REQUIRE(c != nullptr);
    ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
    EP_REQUIRE(instance != nullptr);

    float speed = 0.0f;
    instance->GetFieldValue(FieldIndex(*c, "Speed"), &speed);
    EXPECT_NEAR(9.0f, speed, 1e-5f);

    engine.OnDestroyEntity(entity);
    engine.RemoveFieldMap(entity.GetUUID());
}

// Editing a field during play mutates the live instance directly and bypasses
// the side table, so stopping and replaying restores the editor-time value.
TEST(Scripting, ScriptEngine_LiveFieldEdits_DiscardedOnReplay)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = scene->CreateEntity("Scripted");
    entity.AddComponent<ScriptComponent>(std::string(kUserClass));

    ScriptFieldValue stored;
    stored.Type = ScriptFieldType::Float;
    stored.Set(3.0f);
    engine.GetFieldMap(entity.GetUUID())["Speed"] = stored;

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const int32_t speedIndex = FieldIndex(*c, "Speed");

    engine.OnCreateEntity(entity);
    {
        ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
        EP_REQUIRE(instance != nullptr);
        float live = 50.0f;
        instance->SetFieldValue(speedIndex, &live); // edit during play
        float check = 0.0f;
        instance->GetFieldValue(speedIndex, &check);
        EXPECT_NEAR(50.0f, check, 1e-5f); // edit took on the live instance
    }
    engine.OnDestroyEntity(entity); // stop

    engine.OnCreateEntity(entity); // replay
    {
        ScriptInstance* instance = engine.GetEntityInstance(entity.GetUUID());
        EP_REQUIRE(instance != nullptr);
        float restored = 0.0f;
        instance->GetFieldValue(speedIndex, &restored);
        EXPECT_NEAR(3.0f, restored, 1e-5f); // side-table value, live edit gone
    }
    engine.OnDestroyEntity(entity);
    engine.RemoveFieldMap(entity.GetUUID());
}

// --- Runtime lifecycle: Scene publishes the scripting scene context, so
// OnCreate and OnDestroy both run against a resolvable scene and a live
// physics world. Asserted through native scene/physics state, since the
// managed instance is destroyed as OnDestroy returns. ---

TEST(Scripting, Scene_OnRuntimeStart_RunsOnCreateWithSceneAndPhysicsContext)
{
    EP_REQUIRE(EnsureRuntime());

    // Scene must publish the context itself; don't inherit one from a prior test.
    ScriptEngine::Get().SetSceneContext(nullptr);

    const Ref<Scene> scene = CreateRef<Scene>();
    Entity entity = MakeLifecycleBody(scene);

    scene->OnRuntimeStart();

    // Component write from OnCreate landed on the native transform.
    CHECK_VEC3_CLOSE(glm::vec3(1.0f, 2.0f, 3.0f), entity.GetComponent<TransformComponent>().Translation, 1e-5f);

    // Scene op from OnCreate produced a real entity.
    EXPECT_TRUE(HasEntityNamed(scene, "CreatedFromOnCreate"));

    // Physics op from OnCreate reached the live body.
    EP_REQUIRE(scene->GetPhysicsWorld() != nullptr);
    EXPECT_TRUE(scene->GetPhysicsWorld()->GetLinearVelocity(entity.GetUUID()).y > 0.0f);

    scene->OnRuntimeStop();
}

TEST(Scripting, Scene_OnRuntimeStop_RunsOnDestroyWithSceneAndPhysicsContext)
{
    EP_REQUIRE(EnsureRuntime());

    ScriptEngine::Get().SetSceneContext(nullptr);

    const Ref<Scene> scene = CreateRef<Scene>();
    Entity entity = MakeLifecycleBody(scene);

    scene->OnRuntimeStart();
    scene->OnRuntimeStop();

    // Scene op from OnDestroy produced a real entity.
    EXPECT_TRUE(HasEntityNamed(scene, "CreatedFromOnDestroy"));

    // OnDestroy overwrote the translation with the velocity it read back, so a
    // non-zero y proves the physics world was still live. Zero would mean the
    // scene resolved but physics was already released; OnCreate's (1,2,3) that
    // neither resolved.
    EXPECT_TRUE(entity.GetComponent<TransformComponent>().Translation.y > 0.0f);
}

// A successful explicit load is what makes the assembly valid for play mode.
TEST(Scripting, ScriptEngine_IsUserAssemblyValid_AfterExplicitLoad_IsTrue)
{
    EP_REQUIRE(EnsureRuntime());

    EXPECT_EQ(true, ScriptEngine::IsUserAssemblyValid());
}

// Hot reload polls every frame, including while no project is open.
TEST(Scripting, ScriptEngine_OnUpdate_WithoutAnActiveProject_DoesNothing)
{
    EP_REQUIRE(EnsureRuntime());

    ScriptEngine::Get().VerifyRuntime();

    EXPECT_EQ(true, ScriptEngine::IsUserAssemblyValid());
}

TEST(Scripting, ScriptEngine_ReloadProjectAssembly_WithoutAnActiveProject_Fails)
{
    EP_REQUIRE(EnsureRuntime());

    EXPECT_EQ(false, ScriptEngine::Get().ReloadProjectAssembly());
}

// Editor-managed projects live under the root directory, so a script build that
// writes above them compiles an empty assembly: the .NET SDK excludes everything
// under OutputPath from the default compile glob, and reports success anyway.
TEST(Scripting, ScriptEngine_ReloadProjectAssembly_ForProjectUnderTheProjectsDirectory_DiscoversScriptClasses)
{
    EP_REQUIRE(EnsureRuntime());

    const Testing::TempDir projectDirectory(Project::GetProjectsDirectory());
    const auto scriptsDirectory = projectDirectory.Path() / "Scripts";
    std::filesystem::create_directories(scriptsDirectory / "Source");

    // Mirrors the new-project template: default compile items, core resolved
    // through the CoreManagedDll property the engine passes.
    EP_REQUIRE(
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

    EP_REQUIRE(
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

    EXPECT_EQ(true, reloaded);
    EXPECT_EQ(true, discovered);
}

// Covers the addition case only, and passes with or without the snapshot: entt's
// storage is paged and views iterate in reverse, so appends fall outside the walk.
// Component removal is the genuinely unsafe mutation and is not covered here.
TEST(Scripting, Scene_ScriptSpawnsScriptedEntitiesFromLifecycleHooks_Succeeds)
{
    EP_REQUIRE(EnsureRuntime());

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
    EXPECT_EQ(size_t(8), spawnedFromCreate);

    scene->OnRuntimeStop();

    size_t spawnedFromDestroy = 0;
    scene->ForEachEntity(
        [&](Entity e)
        {
            if (e.GetName().starts_with("SpawnedFromDestroy"))
                spawnedFromDestroy++;
        }
    );
    EXPECT_EQ(size_t(8), spawnedFromDestroy);
}

TEST(Scripting, Scene_OnRuntimeStop_ClearsSceneContext)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    MakeLifecycleBody(scene);

    scene->OnRuntimeStart();
    EXPECT_TRUE(ScriptEngine::Get().GetSceneContext() == scene);

    scene->OnRuntimeStop();
    EXPECT_TRUE(ScriptEngine::Get().GetSceneContext() == nullptr);
}

// --- Internal calls (ScriptGlue): each test invokes the 1:1 harness forwarder for one
// internal call and asserts on native state / the returned value. ---

// --- Writable mesh handles: a script can make a spawned entity visible. ---

TEST(Scripting, MeshComponent_SetMeshHandle_AssignsPrimitiveHandle)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    entity.AddComponent<MeshComponent>();

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* setHandle = c->GetMethod("MeshComponent_SetMeshHandle");
    EP_REQUIRE(setHandle != nullptr);

    const uint64_t cube = static_cast<uint64_t>(MeshPrimitiveType::Cube);
    c->InvokeMethod(entity, *setHandle, &cube, nullptr);

    EXPECT_TRUE(entity.GetComponent<MeshComponent>().MeshHandle == AssetHandle(cube));

    engine.OnDestroyEntity(entity);
}

// The typed enum is the API scripts are meant to use; it must land on the same
// reserved handle the asset manager generates primitives from.
TEST(Scripting, MeshComponent_SetPrimitive_MapsEnumToReservedHandle)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    entity.AddComponent<MeshComponent>();

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* setPrimitive = c->GetMethod("MeshComponent_SetPrimitiveCube");
    EP_REQUIRE(setPrimitive != nullptr);

    c->InvokeMethod(entity, *setPrimitive, nullptr, nullptr);

    const AssetHandle assigned = entity.GetComponent<MeshComponent>().MeshHandle;
    EXPECT_TRUE(assigned == AssetHandle(static_cast<uint64_t>(MeshPrimitiveType::Cube)));

    engine.OnDestroyEntity(entity);
}

// Without an active project an unregistered handle can't be verified as a mesh,
// so it must be refused rather than assigned on trust.
TEST(Scripting, MeshComponent_SetMeshHandle_UnknownHandle_LeavesComponentUnchanged)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    entity.AddComponent<MeshComponent>();

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* setHandle = c->GetMethod("MeshComponent_SetMeshHandle");
    EP_REQUIRE(setHandle != nullptr);

    // Seed a known-good value so a rejected assignment is distinguishable.
    const uint64_t sphere = static_cast<uint64_t>(MeshPrimitiveType::Sphere);
    c->InvokeMethod(entity, *setHandle, &sphere, nullptr);

    const uint64_t bogus = 0xDEADBEEFull;
    c->InvokeMethod(entity, *setHandle, &bogus, nullptr);

    EXPECT_TRUE(entity.GetComponent<MeshComponent>().MeshHandle == AssetHandle(sphere));

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, MeshComponent_SetMeshHandle_ZeroClearsAssignment)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    entity.AddComponent<MeshComponent>();

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* setHandle = c->GetMethod("MeshComponent_SetMeshHandle");
    EP_REQUIRE(setHandle != nullptr);

    const uint64_t cube = static_cast<uint64_t>(MeshPrimitiveType::Cube);
    c->InvokeMethod(entity, *setHandle, &cube, nullptr);

    const uint64_t none = 0;
    c->InvokeMethod(entity, *setHandle, &none, nullptr);

    EXPECT_TRUE(entity.GetComponent<MeshComponent>().MeshHandle == AssetHandle(0));

    engine.OnDestroyEntity(entity);
}

// The end-to-end case the feature exists for: spawn an entity from a script and
// give it a visible mesh, with no asset handle known to the script author.
TEST(Scripting, Scene_CreateEntity_WithPrimitiveMesh_IsVisibleFromScript)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* spawn = c->GetMethod("Scene_CreateEntityWithPrimitive");
    EP_REQUIRE(spawn != nullptr);

    uint64_t spawnedId = 0;
    c->InvokeMethod(entity, *spawn, nullptr, &spawnedId);

    Entity spawned = scene->GetEntityByUUID(Eppo::UUID(spawnedId));
    EP_REQUIRE(static_cast<bool>(spawned));
    EP_REQUIRE(spawned.HasComponent<MeshComponent>());
    EXPECT_TRUE(spawned.GetComponent<MeshComponent>().MeshHandle == AssetHandle(static_cast<uint64_t>(MeshPrimitiveType::Sphere)));

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, LogMessage_NativeCallback_DoesNotCrashHost)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeLiveEntity(scene);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* logMessage = c->GetMethod("LogMessage");
    EP_REQUIRE(logMessage != nullptr);

    c->InvokeMethod(entity, *logMessage, nullptr, nullptr);
    EXPECT_TRUE(true); // reached here: the native Log callback did not fault

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, Entity_HasComponent_ReflectsScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* hasComponent = c->GetMethod("Entity_HasComponent");
    EP_REQUIRE(hasComponent != nullptr);

    bool before = true;
    c->InvokeMethod(entity, *hasComponent, nullptr, &before);
    EXPECT_EQ(false, before);

    entity.AddComponent<PointLightComponent>();
    bool after = false;
    c->InvokeMethod(entity, *hasComponent, nullptr, &after);
    EXPECT_EQ(true, after);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, Entity_AddComponent_AddsToScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    EXPECT_TRUE(!entity.HasComponent<PointLightComponent>());

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* addComponent = c->GetMethod("Entity_AddComponent");
    EP_REQUIRE(addComponent != nullptr);

    c->InvokeMethod(entity, *addComponent, nullptr, nullptr);
    EXPECT_TRUE(entity.HasComponent<PointLightComponent>());

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, Entity_RemoveComponent_RemovesFromScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    entity.AddComponent<PointLightComponent>();

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* removeComponent = c->GetMethod("Entity_RemoveComponent");
    EP_REQUIRE(removeComponent != nullptr);

    bool removed = false;
    c->InvokeMethod(entity, *removeComponent, nullptr, &removed);
    EXPECT_EQ(true, removed);
    EXPECT_TRUE(!entity.HasComponent<PointLightComponent>());

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, Entity_GetName_MarshalsStringToManaged)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    // The harness forwarder compares the native name against this exact string.
    entity.GetComponent<TagComponent>().Tag = "NamedEntity";

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* getName = c->GetMethod("Entity_GetName_Matches");
    EP_REQUIRE(getName != nullptr);

    bool matches = false;
    c->InvokeMethod(entity, *getName, nullptr, &matches);
    EXPECT_EQ(true, matches);

    // Negative case: a different name must decode differently, proving the
    // forwarder returns the real marshalled string rather than a constant.
    entity.GetComponent<TagComponent>().Tag = "Other";
    bool mismatches = true;
    c->InvokeMethod(entity, *getName, nullptr, &mismatches);
    EXPECT_EQ(false, mismatches);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, TransformComponent_GetTranslation_ReturnsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    entity.GetComponent<TransformComponent>().Translation = glm::vec3(1.0f, 2.0f, 3.0f);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* getTranslation = c->GetMethod("TransformComponent_GetTranslation");
    EP_REQUIRE(getTranslation != nullptr);

    glm::vec3 translation{};
    c->InvokeMethod(entity, *getTranslation, nullptr, &translation);
    CHECK_VEC3_CLOSE(glm::vec3(1.0f, 2.0f, 3.0f), translation, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, TransformComponent_SetTranslation_MutatesScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* setTranslation = c->GetMethod("TransformComponent_SetTranslation");
    EP_REQUIRE(setTranslation != nullptr);

    const float in[3] = { 4.0f, 5.0f, 6.0f };
    c->InvokeMethod(entity, *setTranslation, in, nullptr);

    const glm::vec3 t = entity.GetComponent<TransformComponent>().Translation;
    EXPECT_NEAR(4.0f, t.x, 1e-5f);
    EXPECT_NEAR(5.0f, t.y, 1e-5f);
    EXPECT_NEAR(6.0f, t.z, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, MeshComponent_GetMeshHandle_ReturnsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    entity.AddComponent<MeshComponent>(AssetHandle(0x1234ull));

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* getHandle = c->GetMethod("MeshComponent_GetMeshHandle");
    EP_REQUIRE(getHandle != nullptr);

    uint64_t handle = 0;
    c->InvokeMethod(entity, *getHandle, nullptr, &handle);
    EXPECT_TRUE(handle == 0x1234ull);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, PointLightComponent_SetColor_MutatesScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    entity.AddComponent<PointLightComponent>();

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* setColor = c->GetMethod("PointLightComponent_SetColor");
    EP_REQUIRE(setColor != nullptr);

    const float color[3] = { 0.25f, 0.5f, 0.75f };
    c->InvokeMethod(entity, *setColor, color, nullptr);

    const glm::vec3 stored = entity.GetComponent<PointLightComponent>().Color;
    EXPECT_NEAR(0.25f, stored.x, 1e-5f);
    EXPECT_NEAR(0.5f, stored.y, 1e-5f);
    EXPECT_NEAR(0.75f, stored.z, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, PointLightComponent_GetColor_ReturnsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    entity.AddComponent<PointLightComponent>().Color = glm::vec3(0.1f, 0.2f, 0.3f);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* getColor = c->GetMethod("PointLightComponent_GetColor");
    EP_REQUIRE(getColor != nullptr);

    glm::vec3 color{};
    c->InvokeMethod(entity, *getColor, nullptr, &color);
    CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), color, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, PointLightComponent_SetIntensity_MutatesScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    entity.AddComponent<PointLightComponent>();

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* setIntensity = c->GetMethod("PointLightComponent_SetIntensity");
    EP_REQUIRE(setIntensity != nullptr);

    const float intensity = 4.5f;
    c->InvokeMethod(entity, *setIntensity, &intensity, nullptr);
    EXPECT_NEAR(4.5f, entity.GetComponent<PointLightComponent>().Intensity, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, PointLightComponent_GetIntensity_ReturnsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    entity.AddComponent<PointLightComponent>().Intensity = 7.25f;

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* getIntensity = c->GetMethod("PointLightComponent_GetIntensity");
    EP_REQUIRE(getIntensity != nullptr);

    float intensity = 0.0f;
    c->InvokeMethod(entity, *getIntensity, nullptr, &intensity);
    EXPECT_NEAR(7.25f, intensity, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, RelationshipComponent_SetParent_MutatesScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity parent = MakeContextEntity(scene);
    Entity child = MakeContextEntity(scene); // CreateEntity gives it a RelationshipComponent

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* setParent = c->GetMethod("RelationshipComponent_SetParent");
    EP_REQUIRE(setParent != nullptr);

    const uint64_t parentId = static_cast<uint64_t>(parent.GetUUID());
    c->InvokeMethod(child, *setParent, &parentId, nullptr);

    EXPECT_TRUE(child.GetComponent<RelationshipComponent>().Parent == parent.GetUUID());

    engine.OnDestroyEntity(child);
    engine.OnDestroyEntity(parent);
}

TEST(Scripting, RelationshipComponent_GetParent_ReturnsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity parent = MakeContextEntity(scene);
    Entity child = MakeContextEntity(scene);
    scene->SetParent(child, parent);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* getParent = c->GetMethod("RelationshipComponent_GetParent");
    EP_REQUIRE(getParent != nullptr);

    uint64_t parentId = 0;
    c->InvokeMethod(child, *getParent, nullptr, &parentId);
    EXPECT_TRUE(parentId == static_cast<uint64_t>(parent.GetUUID()));

    engine.OnDestroyEntity(child);
    engine.OnDestroyEntity(parent);
}

TEST(Scripting, RigidBodyComponent_GetType_ReturnsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    entity.AddComponent<RigidBodyComponent>().Type = RigidBodyComponent::BodyType::Kinematic;

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* getType = c->GetMethod("RigidBodyComponent_GetType");
    EP_REQUIRE(getType != nullptr);

    uint8_t got = 0;
    c->InvokeMethod(entity, *getType, nullptr, &got);
    EXPECT_EQ(static_cast<int>(RigidBodyComponent::BodyType::Kinematic), static_cast<int>(got));

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, RigidBodyComponent_SetType_MutatesScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& rb = entity.AddComponent<RigidBodyComponent>();
    rb.Type = RigidBodyComponent::BodyType::Kinematic;

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* setType = c->GetMethod("RigidBodyComponent_SetType");
    EP_REQUIRE(setType != nullptr);

    const uint8_t dynamic = static_cast<uint8_t>(RigidBodyComponent::BodyType::Dynamic);
    c->InvokeMethod(entity, *setType, &dynamic, nullptr);
    EXPECT_TRUE(rb.Type == RigidBodyComponent::BodyType::Dynamic);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, BoxColliderComponent_Getters_ReturnSceneValues)
{
    EP_REQUIRE(EnsureRuntime());

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
    EP_REQUIRE(c != nullptr);

    glm::vec3 halfSize{};
    const ScriptMethod* getHalfSize = c->GetMethod("BoxColliderComponent_GetHalfSize");
    EP_REQUIRE(getHalfSize != nullptr);
    c->InvokeMethod(entity, *getHalfSize, nullptr, &halfSize);
    CHECK_VEC3_CLOSE(glm::vec3(1.0f, 2.0f, 3.0f), halfSize, 1e-5f);

    glm::vec3 offset{};
    const ScriptMethod* getOffset = c->GetMethod("BoxColliderComponent_GetOffset");
    EP_REQUIRE(getOffset != nullptr);
    c->InvokeMethod(entity, *getOffset, nullptr, &offset);
    CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), offset, 1e-5f);

    float density = 0.0f;
    const ScriptMethod* getDensity = c->GetMethod("BoxColliderComponent_GetDensity");
    EP_REQUIRE(getDensity != nullptr);
    c->InvokeMethod(entity, *getDensity, nullptr, &density);
    EXPECT_NEAR(2.0f, density, 1e-5f);

    float friction = 0.0f;
    const ScriptMethod* getFriction = c->GetMethod("BoxColliderComponent_GetFriction");
    EP_REQUIRE(getFriction != nullptr);
    c->InvokeMethod(entity, *getFriction, nullptr, &friction);
    EXPECT_NEAR(0.25f, friction, 1e-5f);

    float restitution = 0.0f;
    const ScriptMethod* getRestitution = c->GetMethod("BoxColliderComponent_GetRestitution");
    EP_REQUIRE(getRestitution != nullptr);
    c->InvokeMethod(entity, *getRestitution, nullptr, &restitution);
    EXPECT_NEAR(0.1f, restitution, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, BoxColliderComponent_Setters_MutateScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& box = entity.AddComponent<BoxColliderComponent>();

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    const glm::vec3 halfSize(4.0f, 5.0f, 6.0f);
    const ScriptMethod* setHalfSize = c->GetMethod("BoxColliderComponent_SetHalfSize");
    EP_REQUIRE(setHalfSize != nullptr);
    c->InvokeMethod(entity, *setHalfSize, &halfSize, nullptr);
    CHECK_VEC3_CLOSE(halfSize, box.HalfSize, 1e-5f);

    const glm::vec3 offset(0.4f, 0.5f, 0.6f);
    const ScriptMethod* setOffset = c->GetMethod("BoxColliderComponent_SetOffset");
    EP_REQUIRE(setOffset != nullptr);
    c->InvokeMethod(entity, *setOffset, &offset, nullptr);
    CHECK_VEC3_CLOSE(offset, box.Offset, 1e-5f);

    const float density = 3.0f;
    const ScriptMethod* setDensity = c->GetMethod("BoxColliderComponent_SetDensity");
    EP_REQUIRE(setDensity != nullptr);
    c->InvokeMethod(entity, *setDensity, &density, nullptr);
    EXPECT_NEAR(3.0f, box.Density, 1e-5f);

    const float friction = 0.75f;
    const ScriptMethod* setFriction = c->GetMethod("BoxColliderComponent_SetFriction");
    EP_REQUIRE(setFriction != nullptr);
    c->InvokeMethod(entity, *setFriction, &friction, nullptr);
    EXPECT_NEAR(0.75f, box.Friction, 1e-5f);

    const float restitution = 0.9f;
    const ScriptMethod* setRestitution = c->GetMethod("BoxColliderComponent_SetRestitution");
    EP_REQUIRE(setRestitution != nullptr);
    c->InvokeMethod(entity, *setRestitution, &restitution, nullptr);
    EXPECT_NEAR(0.9f, box.Restitution, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, SphereColliderComponent_Getters_ReturnSceneValues)
{
    EP_REQUIRE(EnsureRuntime());

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
    EP_REQUIRE(c != nullptr);

    float radius = 0.0f;
    const ScriptMethod* getRadius = c->GetMethod("SphereColliderComponent_GetRadius");
    EP_REQUIRE(getRadius != nullptr);
    c->InvokeMethod(entity, *getRadius, nullptr, &radius);
    EXPECT_NEAR(1.5f, radius, 1e-5f);

    glm::vec3 offset{};
    const ScriptMethod* getOffset = c->GetMethod("SphereColliderComponent_GetOffset");
    EP_REQUIRE(getOffset != nullptr);
    c->InvokeMethod(entity, *getOffset, nullptr, &offset);
    CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), offset, 1e-5f);

    float density = 0.0f;
    const ScriptMethod* getDensity = c->GetMethod("SphereColliderComponent_GetDensity");
    EP_REQUIRE(getDensity != nullptr);
    c->InvokeMethod(entity, *getDensity, nullptr, &density);
    EXPECT_NEAR(2.0f, density, 1e-5f);

    float friction = 0.0f;
    const ScriptMethod* getFriction = c->GetMethod("SphereColliderComponent_GetFriction");
    EP_REQUIRE(getFriction != nullptr);
    c->InvokeMethod(entity, *getFriction, nullptr, &friction);
    EXPECT_NEAR(0.25f, friction, 1e-5f);

    float restitution = 0.0f;
    const ScriptMethod* getRestitution = c->GetMethod("SphereColliderComponent_GetRestitution");
    EP_REQUIRE(getRestitution != nullptr);
    c->InvokeMethod(entity, *getRestitution, nullptr, &restitution);
    EXPECT_NEAR(0.1f, restitution, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, SphereColliderComponent_Setters_MutateScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& sphere = entity.AddComponent<SphereColliderComponent>();

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    const float radius = 2.5f;
    const ScriptMethod* setRadius = c->GetMethod("SphereColliderComponent_SetRadius");
    EP_REQUIRE(setRadius != nullptr);
    c->InvokeMethod(entity, *setRadius, &radius, nullptr);
    EXPECT_NEAR(2.5f, sphere.Radius, 1e-5f);

    const glm::vec3 offset(0.4f, 0.5f, 0.6f);
    const ScriptMethod* setOffset = c->GetMethod("SphereColliderComponent_SetOffset");
    EP_REQUIRE(setOffset != nullptr);
    c->InvokeMethod(entity, *setOffset, &offset, nullptr);
    CHECK_VEC3_CLOSE(offset, sphere.Offset, 1e-5f);

    const float density = 3.0f;
    const ScriptMethod* setDensity = c->GetMethod("SphereColliderComponent_SetDensity");
    EP_REQUIRE(setDensity != nullptr);
    c->InvokeMethod(entity, *setDensity, &density, nullptr);
    EXPECT_NEAR(3.0f, sphere.Density, 1e-5f);

    const float friction = 0.75f;
    const ScriptMethod* setFriction = c->GetMethod("SphereColliderComponent_SetFriction");
    EP_REQUIRE(setFriction != nullptr);
    c->InvokeMethod(entity, *setFriction, &friction, nullptr);
    EXPECT_NEAR(0.75f, sphere.Friction, 1e-5f);

    const float restitution = 0.9f;
    const ScriptMethod* setRestitution = c->GetMethod("SphereColliderComponent_SetRestitution");
    EP_REQUIRE(setRestitution != nullptr);
    c->InvokeMethod(entity, *setRestitution, &restitution, nullptr);
    EXPECT_NEAR(0.9f, sphere.Restitution, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, CapsuleColliderComponent_Getters_ReturnSceneValues)
{
    EP_REQUIRE(EnsureRuntime());

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
    EP_REQUIRE(c != nullptr);

    float radius = 0.0f;
    const ScriptMethod* getRadius = c->GetMethod("CapsuleColliderComponent_GetRadius");
    EP_REQUIRE(getRadius != nullptr);
    c->InvokeMethod(entity, *getRadius, nullptr, &radius);
    EXPECT_NEAR(1.5f, radius, 1e-5f);

    float height = 0.0f;
    const ScriptMethod* getHeight = c->GetMethod("CapsuleColliderComponent_GetHeight");
    EP_REQUIRE(getHeight != nullptr);
    c->InvokeMethod(entity, *getHeight, nullptr, &height);
    EXPECT_NEAR(3.0f, height, 1e-5f);

    glm::vec3 offset{};
    const ScriptMethod* getOffset = c->GetMethod("CapsuleColliderComponent_GetOffset");
    EP_REQUIRE(getOffset != nullptr);
    c->InvokeMethod(entity, *getOffset, nullptr, &offset);
    CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), offset, 1e-5f);

    float density = 0.0f;
    const ScriptMethod* getDensity = c->GetMethod("CapsuleColliderComponent_GetDensity");
    EP_REQUIRE(getDensity != nullptr);
    c->InvokeMethod(entity, *getDensity, nullptr, &density);
    EXPECT_NEAR(2.0f, density, 1e-5f);

    float friction = 0.0f;
    const ScriptMethod* getFriction = c->GetMethod("CapsuleColliderComponent_GetFriction");
    EP_REQUIRE(getFriction != nullptr);
    c->InvokeMethod(entity, *getFriction, nullptr, &friction);
    EXPECT_NEAR(0.25f, friction, 1e-5f);

    float restitution = 0.0f;
    const ScriptMethod* getRestitution = c->GetMethod("CapsuleColliderComponent_GetRestitution");
    EP_REQUIRE(getRestitution != nullptr);
    c->InvokeMethod(entity, *getRestitution, nullptr, &restitution);
    EXPECT_NEAR(0.1f, restitution, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, CapsuleColliderComponent_Setters_MutateScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& capsule = entity.AddComponent<CapsuleColliderComponent>();

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    const float radius = 2.5f;
    const ScriptMethod* setRadius = c->GetMethod("CapsuleColliderComponent_SetRadius");
    EP_REQUIRE(setRadius != nullptr);
    c->InvokeMethod(entity, *setRadius, &radius, nullptr);
    EXPECT_NEAR(2.5f, capsule.Radius, 1e-5f);

    const float height = 4.0f;
    const ScriptMethod* setHeight = c->GetMethod("CapsuleColliderComponent_SetHeight");
    EP_REQUIRE(setHeight != nullptr);
    c->InvokeMethod(entity, *setHeight, &height, nullptr);
    EXPECT_NEAR(4.0f, capsule.Height, 1e-5f);

    const glm::vec3 offset(0.4f, 0.5f, 0.6f);
    const ScriptMethod* setOffset = c->GetMethod("CapsuleColliderComponent_SetOffset");
    EP_REQUIRE(setOffset != nullptr);
    c->InvokeMethod(entity, *setOffset, &offset, nullptr);
    CHECK_VEC3_CLOSE(offset, capsule.Offset, 1e-5f);

    const float density = 3.0f;
    const ScriptMethod* setDensity = c->GetMethod("CapsuleColliderComponent_SetDensity");
    EP_REQUIRE(setDensity != nullptr);
    c->InvokeMethod(entity, *setDensity, &density, nullptr);
    EXPECT_NEAR(3.0f, capsule.Density, 1e-5f);

    const float friction = 0.75f;
    const ScriptMethod* setFriction = c->GetMethod("CapsuleColliderComponent_SetFriction");
    EP_REQUIRE(setFriction != nullptr);
    c->InvokeMethod(entity, *setFriction, &friction, nullptr);
    EXPECT_NEAR(0.75f, capsule.Friction, 1e-5f);

    const float restitution = 0.9f;
    const ScriptMethod* setRestitution = c->GetMethod("CapsuleColliderComponent_SetRestitution");
    EP_REQUIRE(setRestitution != nullptr);
    c->InvokeMethod(entity, *setRestitution, &restitution, nullptr);
    EXPECT_NEAR(0.9f, capsule.Restitution, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, CylinderColliderComponent_GettersAndSetters_RoundTripSceneValues)
{
    EP_REQUIRE(EnsureRuntime());

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
    EP_REQUIRE(c != nullptr);

    float radius = 0.0f;
    const ScriptMethod* getRadius = c->GetMethod("CylinderColliderComponent_GetRadius");
    EP_REQUIRE(getRadius != nullptr);
    c->InvokeMethod(entity, *getRadius, nullptr, &radius);
    EXPECT_NEAR(1.5f, radius, 1e-5f);

    float height = 0.0f;
    const ScriptMethod* getHeight = c->GetMethod("CylinderColliderComponent_GetHeight");
    EP_REQUIRE(getHeight != nullptr);
    c->InvokeMethod(entity, *getHeight, nullptr, &height);
    EXPECT_NEAR(3.0f, height, 1e-5f);

    glm::vec3 offset{};
    const ScriptMethod* getOffset = c->GetMethod("CylinderColliderComponent_GetOffset");
    EP_REQUIRE(getOffset != nullptr);
    c->InvokeMethod(entity, *getOffset, nullptr, &offset);
    CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), offset, 1e-5f);

    float density = 0.0f;
    const ScriptMethod* getDensity = c->GetMethod("CylinderColliderComponent_GetDensity");
    EP_REQUIRE(getDensity != nullptr);
    c->InvokeMethod(entity, *getDensity, nullptr, &density);
    EXPECT_NEAR(2.0f, density, 1e-5f);

    float friction = 0.0f;
    const ScriptMethod* getFriction = c->GetMethod("CylinderColliderComponent_GetFriction");
    EP_REQUIRE(getFriction != nullptr);
    c->InvokeMethod(entity, *getFriction, nullptr, &friction);
    EXPECT_NEAR(0.25f, friction, 1e-5f);

    float restitution = 0.0f;
    const ScriptMethod* getRestitution = c->GetMethod("CylinderColliderComponent_GetRestitution");
    EP_REQUIRE(getRestitution != nullptr);
    c->InvokeMethod(entity, *getRestitution, nullptr, &restitution);
    EXPECT_NEAR(0.1f, restitution, 1e-5f);

    radius = 2.5f;
    const ScriptMethod* setRadius = c->GetMethod("CylinderColliderComponent_SetRadius");
    EP_REQUIRE(setRadius != nullptr);
    c->InvokeMethod(entity, *setRadius, &radius, nullptr);
    EXPECT_NEAR(2.5f, cylinder.Radius, 1e-5f);

    height = 4.0f;
    const ScriptMethod* setHeight = c->GetMethod("CylinderColliderComponent_SetHeight");
    EP_REQUIRE(setHeight != nullptr);
    c->InvokeMethod(entity, *setHeight, &height, nullptr);
    EXPECT_NEAR(4.0f, cylinder.Height, 1e-5f);

    offset = glm::vec3(0.4f, 0.5f, 0.6f);
    const ScriptMethod* setOffset = c->GetMethod("CylinderColliderComponent_SetOffset");
    EP_REQUIRE(setOffset != nullptr);
    c->InvokeMethod(entity, *setOffset, &offset, nullptr);
    CHECK_VEC3_CLOSE(offset, cylinder.Offset, 1e-5f);

    density = 3.0f;
    const ScriptMethod* setDensity = c->GetMethod("CylinderColliderComponent_SetDensity");
    EP_REQUIRE(setDensity != nullptr);
    c->InvokeMethod(entity, *setDensity, &density, nullptr);
    EXPECT_NEAR(3.0f, cylinder.Density, 1e-5f);

    friction = 0.75f;
    const ScriptMethod* setFriction = c->GetMethod("CylinderColliderComponent_SetFriction");
    EP_REQUIRE(setFriction != nullptr);
    c->InvokeMethod(entity, *setFriction, &friction, nullptr);
    EXPECT_NEAR(0.75f, cylinder.Friction, 1e-5f);

    restitution = 0.9f;
    const ScriptMethod* setRestitution = c->GetMethod("CylinderColliderComponent_SetRestitution");
    EP_REQUIRE(setRestitution != nullptr);
    c->InvokeMethod(entity, *setRestitution, &restitution, nullptr);
    EXPECT_NEAR(0.9f, cylinder.Restitution, 1e-5f);

    engine.OnDestroyEntity(entity);
}

// LinearVelocity routes through the active physics world rather than the component.
TEST(Scripting, RigidBodyComponent_GetLinearVelocity_ReturnsWorldValue)
{
    EP_REQUIRE(EnsureRuntime());

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
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* getVelocity = c->GetMethod("RigidBodyComponent_GetLinearVelocity");
    EP_REQUIRE(getVelocity != nullptr);

    glm::vec3 velocity{};
    c->InvokeMethod(entity, *getVelocity, nullptr, &velocity);
    CHECK_VEC3_CLOSE(glm::vec3(1.0f, 2.0f, 3.0f), velocity, 1e-4f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, RigidBodyComponent_SetLinearVelocity_MutatesWorld)
{
    EP_REQUIRE(EnsureRuntime());

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
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* setVelocity = c->GetMethod("RigidBodyComponent_SetLinearVelocity");
    EP_REQUIRE(setVelocity != nullptr);

    const glm::vec3 velocity(1.0f, 2.0f, 3.0f);
    c->InvokeMethod(entity, *setVelocity, &velocity, nullptr);
    CHECK_VEC3_CLOSE(velocity, world->GetLinearVelocity(entity.GetUUID()), 1e-4f);

    engine.OnDestroyEntity(entity);
}

// Physics.ApplyLinearImpulse (static) drives the body via the active world.
TEST(Scripting, Physics_ApplyLinearImpulse_AddsVelocity)
{
    EP_REQUIRE(EnsureRuntime());

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
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* impulse = c->GetMethod("Physics_ApplyLinearImpulseUp");
    EP_REQUIRE(impulse != nullptr);

    c->InvokeMethod(entity, *impulse, nullptr, nullptr);
    world->Step(1.0f / 60.0f);
    EXPECT_TRUE(world->GetLinearVelocity(entity.GetUUID()).y > 0.0f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, TransformComponent_Rotation_RoundTripsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& tc = entity.GetComponent<TransformComponent>();
    tc.Rotation = glm::vec3(0.1f, 0.2f, 0.3f);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    glm::vec3 rotation{};
    const ScriptMethod* getRotation = c->GetMethod("TransformComponent_GetRotation");
    EP_REQUIRE(getRotation != nullptr);
    c->InvokeMethod(entity, *getRotation, nullptr, &rotation);
    CHECK_VEC3_CLOSE(glm::vec3(0.1f, 0.2f, 0.3f), rotation, 1e-5f);

    const glm::vec3 next(0.4f, 0.5f, 0.6f);
    const ScriptMethod* setRotation = c->GetMethod("TransformComponent_SetRotation");
    EP_REQUIRE(setRotation != nullptr);
    c->InvokeMethod(entity, *setRotation, &next, nullptr);
    CHECK_VEC3_CLOSE(next, tc.Rotation, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, TransformComponent_Scale_RoundTripsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& tc = entity.GetComponent<TransformComponent>();
    tc.Scale = glm::vec3(2.0f, 3.0f, 4.0f);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    glm::vec3 scale{};
    const ScriptMethod* getScale = c->GetMethod("TransformComponent_GetScale");
    EP_REQUIRE(getScale != nullptr);
    c->InvokeMethod(entity, *getScale, nullptr, &scale);
    CHECK_VEC3_CLOSE(glm::vec3(2.0f, 3.0f, 4.0f), scale, 1e-5f);

    const glm::vec3 next(0.5f, 0.25f, 0.125f);
    const ScriptMethod* setScale = c->GetMethod("TransformComponent_SetScale");
    EP_REQUIRE(setScale != nullptr);
    c->InvokeMethod(entity, *setScale, &next, nullptr);
    CHECK_VEC3_CLOSE(next, tc.Scale, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, RigidBodyComponent_GravityScale_RoundTripsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& rb = entity.AddComponent<RigidBodyComponent>();
    rb.GravityScale = 2.5f;

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    float gravityScale = 0.0f;
    const ScriptMethod* getGravity = c->GetMethod("RigidBodyComponent_GetGravityScale");
    EP_REQUIRE(getGravity != nullptr);
    c->InvokeMethod(entity, *getGravity, nullptr, &gravityScale);
    EXPECT_NEAR(2.5f, gravityScale, 1e-5f);

    const float next = 0.75f;
    const ScriptMethod* setGravity = c->GetMethod("RigidBodyComponent_SetGravityScale");
    EP_REQUIRE(setGravity != nullptr);
    c->InvokeMethod(entity, *setGravity, &next, nullptr);
    EXPECT_NEAR(0.75f, rb.GravityScale, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, RigidBodyComponent_LinearDamping_RoundTripsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& rb = entity.AddComponent<RigidBodyComponent>();
    rb.LinearDamping = 0.3f;

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    float linearDamping = 0.0f;
    const ScriptMethod* getDamping = c->GetMethod("RigidBodyComponent_GetLinearDamping");
    EP_REQUIRE(getDamping != nullptr);
    c->InvokeMethod(entity, *getDamping, nullptr, &linearDamping);
    EXPECT_NEAR(0.3f, linearDamping, 1e-5f);

    const float next = 0.9f;
    const ScriptMethod* setDamping = c->GetMethod("RigidBodyComponent_SetLinearDamping");
    EP_REQUIRE(setDamping != nullptr);
    c->InvokeMethod(entity, *setDamping, &next, nullptr);
    EXPECT_NEAR(0.9f, rb.LinearDamping, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, RigidBodyComponent_AngularDamping_RoundTripsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& rb = entity.AddComponent<RigidBodyComponent>();
    rb.AngularDamping = 0.2f;

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    float angularDamping = 0.0f;
    const ScriptMethod* getDamping = c->GetMethod("RigidBodyComponent_GetAngularDamping");
    EP_REQUIRE(getDamping != nullptr);
    c->InvokeMethod(entity, *getDamping, nullptr, &angularDamping);
    EXPECT_NEAR(0.2f, angularDamping, 1e-5f);

    const float next = 0.6f;
    const ScriptMethod* setDamping = c->GetMethod("RigidBodyComponent_SetAngularDamping");
    EP_REQUIRE(setDamping != nullptr);
    c->InvokeMethod(entity, *setDamping, &next, nullptr);
    EXPECT_NEAR(0.6f, rb.AngularDamping, 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, RigidBodyComponent_MotionLocks_RoundTripSceneValues)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& rb = entity.AddComponent<RigidBodyComponent>();
    rb.LockLinearZ = true;
    rb.LockAngularX = true;

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

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
        EP_REQUIRE(getter != nullptr);
        bool got = !expected;
        c->InvokeMethod(entity, *getter, nullptr, &got);
        EXPECT_EQ(expected, got);
    }

    const char* setters[] = {
        "RigidBodyComponent_SetLockLinearX",  "RigidBodyComponent_SetLockLinearY",  "RigidBodyComponent_SetLockLinearZ",
        "RigidBodyComponent_SetLockAngularX", "RigidBodyComponent_SetLockAngularY", "RigidBodyComponent_SetLockAngularZ",
    };
    for (const char* name : setters)
    {
        const ScriptMethod* setter = c->GetMethod(name);
        EP_REQUIRE(setter != nullptr);
        bool next = true;
        c->InvokeMethod(entity, *setter, &next, nullptr);
    }

    EXPECT_TRUE(rb.LockLinearX && rb.LockLinearY && rb.LockLinearZ);
    EXPECT_TRUE(rb.LockAngularX && rb.LockAngularY && rb.LockAngularZ);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, Entity_SetName_MutatesScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    // The harness forwarder writes this exact string via Entity_SetName.
    const ScriptMethod* setName = c->GetMethod("Entity_SetName");
    EP_REQUIRE(setName != nullptr);

    c->InvokeMethod(entity, *setName, nullptr, nullptr);
    EXPECT_EQ(std::string("RenamedFromScript"), entity.GetName());

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, RelationshipComponent_GetChildren_ReturnsSceneChildren)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity parent = MakeContextEntity(scene); // scripted invoker
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    scene->SetParent(first, parent);
    scene->SetParent(second, parent);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    int32_t count = 0;
    const ScriptMethod* getCount = c->GetMethod("RelationshipComponent_GetChildCount");
    EP_REQUIRE(getCount != nullptr);
    c->InvokeMethod(parent, *getCount, nullptr, &count);
    EXPECT_EQ(2, count);

    const ScriptMethod* getChild = c->GetMethod("RelationshipComponent_GetChild");
    EP_REQUIRE(getChild != nullptr);
    const int32_t zero = 0;
    const int32_t one = 1;
    uint64_t child0 = 0;
    uint64_t child1 = 0;
    c->InvokeMethod(parent, *getChild, &zero, &child0);
    c->InvokeMethod(parent, *getChild, &one, &child1);

    const uint64_t firstId = static_cast<uint64_t>(first.GetUUID());
    const uint64_t secondId = static_cast<uint64_t>(second.GetUUID());
    EXPECT_TRUE((child0 == firstId && child1 == secondId) || (child0 == secondId && child1 == firstId));

    engine.OnDestroyEntity(parent);
}

TEST(Scripting, CameraComponent_VerticalFov_RoundTripsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& camera = entity.AddComponent<CameraComponent>();
    camera.Camera.SetPerspectiveVerticalFov(60.0f);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    float fov = 0.0f;
    const ScriptMethod* getFov = c->GetMethod("CameraComponent_GetVerticalFov");
    EP_REQUIRE(getFov != nullptr);
    c->InvokeMethod(entity, *getFov, nullptr, &fov);
    EXPECT_NEAR(60.0f, fov, 1e-4f);

    const float next = 75.0f;
    const ScriptMethod* setFov = c->GetMethod("CameraComponent_SetVerticalFov");
    EP_REQUIRE(setFov != nullptr);
    c->InvokeMethod(entity, *setFov, &next, nullptr);
    EXPECT_NEAR(75.0f, camera.Camera.GetPerspectiveVerticalFov(), 1e-4f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, CameraComponent_NearClip_RoundTripsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& camera = entity.AddComponent<CameraComponent>();
    camera.Camera.SetPerspectiveNearClip(0.25f);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    float nearClip = 0.0f;
    const ScriptMethod* getNear = c->GetMethod("CameraComponent_GetNearClip");
    EP_REQUIRE(getNear != nullptr);
    c->InvokeMethod(entity, *getNear, nullptr, &nearClip);
    EXPECT_NEAR(0.25f, nearClip, 1e-5f);

    const float next = 0.5f;
    const ScriptMethod* setNear = c->GetMethod("CameraComponent_SetNearClip");
    EP_REQUIRE(setNear != nullptr);
    c->InvokeMethod(entity, *setNear, &next, nullptr);
    EXPECT_NEAR(0.5f, camera.Camera.GetPerspectiveNearClip(), 1e-5f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, CameraComponent_FarClip_RoundTripsSceneValue)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    auto& camera = entity.AddComponent<CameraComponent>();
    camera.Camera.SetPerspectiveFarClip(500.0f);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);

    float farClip = 0.0f;
    const ScriptMethod* getFar = c->GetMethod("CameraComponent_GetFarClip");
    EP_REQUIRE(getFar != nullptr);
    c->InvokeMethod(entity, *getFar, nullptr, &farClip);
    EXPECT_NEAR(500.0f, farClip, 1e-3f);

    const float next = 250.0f;
    const ScriptMethod* setFar = c->GetMethod("CameraComponent_SetFarClip");
    EP_REQUIRE(setFar != nullptr);
    c->InvokeMethod(entity, *setFar, &next, nullptr);
    EXPECT_NEAR(250.0f, camera.Camera.GetPerspectiveFarClip(), 1e-3f);

    engine.OnDestroyEntity(entity);
}

TEST(Scripting, Scene_CreateEntity_AddsEntityToScene)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity invoker = MakeContextEntity(scene);

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* create = c->GetMethod("Scene_CreateEntity");
    EP_REQUIRE(create != nullptr);

    uint64_t spawnedId = 0;
    c->InvokeMethod(invoker, *create, nullptr, &spawnedId);
    EXPECT_TRUE(spawnedId != 0);

    Entity spawned = scene->GetEntityByUUID(Eppo::UUID(spawnedId));
    EP_REQUIRE(static_cast<bool>(spawned));
    EXPECT_EQ(std::string("Spawned"), spawned.GetName());
    EXPECT_TRUE(spawnedId != static_cast<uint64_t>(invoker.GetUUID()));

    engine.OnDestroyEntity(invoker);
}

TEST(Scripting, Scene_DestroyEntity_DefersUntilFrameEnd)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity invoker = MakeContextEntity(scene);
    Entity doomed = scene->CreateEntity("Doomed");
    const uint64_t doomedId = static_cast<uint64_t>(doomed.GetUUID());

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const ScriptMethod* destroy = c->GetMethod("Scene_DestroyEntity");
    EP_REQUIRE(destroy != nullptr);

    c->InvokeMethod(invoker, *destroy, &doomedId, nullptr);
    // Deferred: the entity is still present until the frame's destroy queue drains.
    EXPECT_TRUE(static_cast<bool>(scene->GetEntityByUUID(Eppo::UUID(doomedId))));

    scene->OnUpdateRuntime(0.0f); // drains the queue after the script update loop
    EXPECT_TRUE(!static_cast<bool>(scene->GetEntityByUUID(Eppo::UUID(doomedId))));

    engine.OnDestroyEntity(invoker);
}

// Destroying a scripted entity must run its managed OnDestroy and unregister the
// live instance, not leak it until engine shutdown.
TEST(Scripting, Scene_DestroyEntity_TearsDownScriptInstance)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity entity = MakeContextEntity(scene);
    const Eppo::UUID id = entity.GetUUID();
    EP_REQUIRE(engine.GetEntityInstance(id) != nullptr);

    scene->DestroyEntity(entity);
    EXPECT_TRUE(engine.GetEntityInstance(id) == nullptr);
}

// A scripted entity destroying itself from inside OnUpdate must not corrupt the
// ScriptComponent view being iterated: the destroy is deferred to the frame's
// drain, the entity is gone afterward, and other scripts still run.
TEST(Scripting, Scene_DestroyEntity_SelfDuringUpdate_IsSafe)
{
    EP_REQUIRE(EnsureRuntime());

    const Ref<Scene> scene = CreateRef<Scene>();
    auto& engine = ScriptEngine::Get();
    Entity selfDestruct = MakeContextEntity(scene);
    Entity survivor = MakeContextEntity(scene); // a second entity in the script view

    const ScriptClass* c = FindClass(kUserClass);
    EP_REQUIRE(c != nullptr);
    const int32_t flagIndex = FieldIndex(*c, "DestroySelfOnUpdate");
    EP_REQUIRE(flagIndex >= 0);

    ScriptInstance* instance = engine.GetEntityInstance(selfDestruct.GetUUID());
    EP_REQUIRE(instance != nullptr);
    bool destroySelf = true;
    instance->SetFieldValue(flagIndex, &destroySelf);

    const Eppo::UUID selfId = selfDestruct.GetUUID();
    const Eppo::UUID survivorId = survivor.GetUUID();

    scene->OnUpdateRuntime(0.016f); // self-destruct queued mid-loop, drained after

    EXPECT_TRUE(!static_cast<bool>(scene->GetEntityByUUID(selfId))); // self-destructed
    EXPECT_TRUE(static_cast<bool>(scene->GetEntityByUUID(survivorId))); // survivor intact
    EXPECT_TRUE(engine.GetEntityInstance(selfId) == nullptr); // instance torn down

    engine.OnDestroyEntity(survivor);
}
