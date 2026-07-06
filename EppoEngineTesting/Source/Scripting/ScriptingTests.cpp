#include "Support/EppoTest.h"

#include "Scripting/ScriptEngine.h"

#include <algorithm>

using namespace Eppo;

// Scripting suite (label `scripting`): boots the hosted .NET runtime from the
// managed core assembly + runtimeconfig.json that CMake copies next to the exe,
// and loads a tiny user assembly (EppoTesting.Scripts.dll) containing one
// concrete script class, EppoTesting.HarnessScript.
//
// The hosted runtime resolves the managed assemblies relative to the working
// directory, so the CTest suites run with their working directory set to the
// exe's folder (see eppo_add_suite). Data files are therefore located via
// FS::GetRootDirectory() (the working directory), matching how the editor
// resolves its own runtimeconfig.
SUITE(Scripting)
{
    constexpr const char* kUserClass = "EppoTesting.HarnessScript";

    namespace
    {
        // CoreCLR cannot be reliably re-initialized within a process, so the
        // whole suite shares a single Init + user-assembly load; teardown happens
        // once, at process exit (ScriptEngine's static instance is destroyed).
        //
        // Returns whether the runtime is actually usable. Init leaves a non-null
        // (but empty) engine instance behind if the .NET host fails to come up,
        // so we verify IsRuntimeLoaded and cache the verdict — every test guards
        // on this to avoid touching a half-initialized engine.
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
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }();
            return ready;
        }

        auto FindClass(const std::string& fullName) -> const EppoScriptCore::ScriptClass*
        {
            const auto& classes = ScriptEngine::Get().GetClasses();
            const auto it = std::ranges::find_if(classes,
                [&](const EppoScriptCore::ScriptClass& c) { return c.GetFullName() == fullName; });
            return it == classes.end() ? nullptr : &*it;
        }
    }

    TEST(RuntimeComesUpWithClassMetadata)
    {
        REQUIRE CHECK(EnsureRuntime());

        CHECK(ScriptEngine::Get().IsRuntimeLoaded());
        // A live runtime with the user assembly loaded exposes discoverable
        // classes; an empty list would mean CoreCLR/metadata never came up.
        CHECK(!ScriptEngine::Get().GetClasses().empty());
    }

    TEST(UserAssemblyExposesItsClass)
    {
        REQUIRE CHECK(EnsureRuntime());

        CHECK(ScriptEngine::Get().IsValidScriptClass(kUserClass));
    }

    TEST(FindClassIndexRoundTrips)
    {
        REQUIRE CHECK(EnsureRuntime());

        const int32_t index = ScriptEngine::Get().FindClassIndex(kUserClass);
        REQUIRE CHECK(index >= 0);

        // The index must point back at the same class.
        const auto& classes = ScriptEngine::Get().GetClasses();
        CHECK_EQUAL(std::string(kUserClass), classes[static_cast<size_t>(index)].GetFullName());
    }

    TEST(ScriptClassExposesPublicFields)
    {
        REQUIRE CHECK(EnsureRuntime());

        const EppoScriptCore::ScriptClass* scriptClass = FindClass(kUserClass);
        REQUIRE CHECK(scriptClass != nullptr);

        // HarnessScript declares `public float Speed` and `public int Count`.
        const auto& fields = scriptClass->GetFields();
        const auto speed = std::ranges::find_if(fields,
            [](const EppoScriptCore::ScriptField& f) { return f.Name == "Speed"; });
        REQUIRE CHECK(speed != fields.end());
        CHECK(speed->Type == EppoScriptCore::ScriptFieldType::Float);

        const auto count = std::ranges::find_if(fields,
            [](const EppoScriptCore::ScriptField& f) { return f.Name == "Count"; });
        REQUIRE CHECK(count != fields.end());
        CHECK(count->Type == EppoScriptCore::ScriptFieldType::Int32);
    }

    TEST(UnknownClassIsNotFound)
    {
        REQUIRE CHECK(EnsureRuntime());

        CHECK_EQUAL(-1, ScriptEngine::Get().FindClassIndex("Nope.DoesNotExist"));
        CHECK(!ScriptEngine::Get().IsValidScriptClass("Nope.DoesNotExist"));
    }
}
