#include "Support/EppoTest.h"

#include "Renderer/RenderPass.h"

using namespace Eppo;

// PassStatistics::operator+= is the pure-CPU accumulation the stats UI relies on
// to build the per-scene subtotal and the scene+UI grand total from individual
// passes (and to aggregate ImGui multi-viewport stats). Constructing a RenderPass
// needs a GPU device, but the statistics arithmetic does not, so it lives in the
// headless `unit` suite.
SUITE(Renderer)
{
    TEST(PassStatisticsAddsFieldwise)
    {
        PassStatistics a{ .DrawCalls = 1, .Meshes = 2, .Submeshes = 3, .Instances = 4, .Vertices = 5, .Indices = 6 };
        const PassStatistics b{ .DrawCalls = 10, .Meshes = 20, .Submeshes = 30, .Instances = 40, .Vertices = 50, .Indices = 60 };

        a += b;

        CHECK_EQUAL(11u, a.DrawCalls);
        CHECK_EQUAL(22u, a.Meshes);
        CHECK_EQUAL(33u, a.Submeshes);
        CHECK_EQUAL(44u, a.Instances);
        CHECK_EQUAL(55u, a.Vertices);
        CHECK_EQUAL(66u, a.Indices);
    }

    TEST(PassStatisticsAddingZeroIsIdentity)
    {
        PassStatistics a{ .DrawCalls = 7, .Vertices = 8, .Indices = 9 };
        a += PassStatistics{};

        CHECK_EQUAL(7u, a.DrawCalls);
        CHECK_EQUAL(8u, a.Vertices);
        CHECK_EQUAL(9u, a.Indices);
    }

    TEST(PassStatisticsAccumulatesAcrossPasses)
    {
        // Mirrors RenderGui summing Geometry + Sky + UI into a grand total.
        const PassStatistics geometry{ .DrawCalls = 12, .Vertices = 300, .Indices = 900 };
        const PassStatistics sky{ .DrawCalls = 1, .Vertices = 3 };
        const PassStatistics ui{ .DrawCalls = 40, .Vertices = 800, .Indices = 1200 };

        PassStatistics total;
        total += geometry;
        total += sky;
        total += ui;

        CHECK_EQUAL(53u, total.DrawCalls);
        CHECK_EQUAL(1103u, total.Vertices);
        CHECK_EQUAL(2100u, total.Indices);
    }

    TEST(PassStatisticsReturnsSelfForChaining)
    {
        PassStatistics a;
        PassStatistics& ref = (a += PassStatistics{ .DrawCalls = 2 });
        ref += PassStatistics{ .DrawCalls = 3 };

        CHECK_EQUAL(&a, &ref);
        CHECK_EQUAL(5u, a.DrawCalls);
    }
}
