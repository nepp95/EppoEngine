#include "Support/EppoTest.h"
#include "Support/TempDir.h"

#include "Project/Project.h"
#include "Project/ProjectSerializer.h"

using namespace Eppo;
using Testing::TempDir;

// Project + ProjectSerializer are headless: New() is pure in-memory and the
// serializer touches only the project spec + disk (no AssetManager, no GPU).
SUITE(Project)
{
    namespace
    {
        auto MakeProject(const std::filesystem::path& dir) -> Ref<Project>
        {
            Ref<Project> project = CreateRef<Project>();
            auto& spec = project->GetSpecification();
            spec.Name = "TestProject";
            spec.ProjectDirectory = dir;
            spec.StartScene = AssetHandle(4242ull);
            return project;
        }
    }

    // Project::New mutates the global s_ActiveProject static. This fixture
    // snapshots and restores it around each case so the suite leaves no global
    // state behind when the whole runner executes in a single process.
    struct ActiveProjectGuard
    {
        ActiveProjectGuard() : m_Saved(Project::GetActive()) {}
        ~ActiveProjectGuard() { Project::SetActive(m_Saved); }

        Ref<Project> m_Saved;
    };

    TEST_FIXTURE(ActiveProjectGuard, NewSpecBecomesActiveProject)
    {
        ProjectSpecification spec;
        spec.Name = "Active";
        Ref<Project> project = Project::New(spec);

        CHECK(Project::GetActive() == project);
        CHECK_EQUAL(std::string("Active"), project->GetSpecification().Name);
    }

    TEST(SerializeWritesEpprojNamedByProject)
    {
        const TempDir dir;
        Ref<Project> project = MakeProject(dir.Path());

        CHECK(ProjectSerializer(project).Serialize());
        CHECK(FS::Exists(dir.File("TestProject.epproj")));
    }

    TEST(RoundTripPreservesSpec)
    {
        const TempDir dir;
        Ref<Project> project = MakeProject(dir.Path());
        CHECK(ProjectSerializer(project).Serialize());

        Ref<Project> loaded = CreateRef<Project>();
        CHECK(ProjectSerializer(loaded).Deserialize(dir.File("TestProject.epproj")));

        const auto& spec = loaded->GetSpecification();
        CHECK_EQUAL(std::string("TestProject"), spec.Name);
        CHECK_EQUAL(4242ull, static_cast<uint64_t>(spec.StartScene));
        CHECK_EQUAL(dir.Path().string(), spec.ProjectDirectory.string());
    }

    TEST(DeserializeMissingFileFails)
    {
        const TempDir dir;
        Ref<Project> loaded = CreateRef<Project>();
        CHECK(!ProjectSerializer(loaded).Deserialize(dir.File("nope.epproj")));
    }
}
