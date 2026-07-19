#include "Support/EppoTest.h"

#include "Renderer/Mesh.h"

#include <filesystem>

using namespace Eppo;

SUITE(Renderer)
{
	TEST(Material_DefaultTextureHandlesProduceInvalidIndices)
	{
		const Material material;

		CHECK_EQUAL(-1, material.GetDiffuseMapIndex());
		CHECK_EQUAL(-1, material.GetNormalMapIndex());
		CHECK_EQUAL(-1, material.GetRoughMetMapIndex());
	}

	TEST(Material_SharedTextureHandleProducesSameGlobalIndex)
	{
		const auto handle = CreateRef<BindlessHandle>();
		handle->Index = 42;

		Material first;
		first.DiffuseMap = handle;
		Material second;
		second.DiffuseMap = handle;

		CHECK_EQUAL(42, first.GetDiffuseMapIndex());
		CHECK_EQUAL(42, second.GetDiffuseMapIndex());
	}
}
