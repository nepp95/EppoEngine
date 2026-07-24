#include "Support/EppoTest.h"
#include "Support/AppHarness.h"
#include "Support/GlmCheck.h"
#include "Support/TempDir.h"

#include "Renderer/Mesh.h"

#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>

using namespace Eppo;

SUITE(Renderer)
{
	namespace
	{
		auto WriteNestedTransformMesh(const Testing::TempDir& tempDir) -> std::filesystem::path
		{
			const std::filesystem::path path = tempDir.File("NestedTransform.gltf");
			const std::string source = R"({
				"asset": { "version": "2.0" },
				"scene": 0,
				"scenes": [
					{ "nodes": [0] },
					{ "nodes": [2] }
				],
				"nodes": [
					{
						"children": [1],
						"matrix": [
							1.0, 0.0, 0.0, 0.0,
							0.0, 0.0, -1.0, 0.0,
							0.0, 1.0, 0.0, 0.0,
							0.0, 0.0, 0.0, 1.0
						]
					},
					{
						"mesh": 0,
						"translation": [2.0, 3.0, 4.0],
						"scale": [2.0, 1.0, 1.0]
					},
					{
						"mesh": 0,
						"translation": [100.0, 0.0, 0.0]
					}
				],
				"meshes": [
					{
						"name": "Triangle",
						"primitives": [
							{
								"attributes": {
									"POSITION": 0,
									"NORMAL": 1,
									"TEXCOORD_0": 2
								},
								"indices": 3
							}
						]
					}
				],
				"buffers": [
					{
						"byteLength": 102,
						"uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAABAAIA"
					}
				],
				"bufferViews": [
					{ "buffer": 0, "byteOffset": 0, "byteLength": 36 },
					{ "buffer": 0, "byteOffset": 36, "byteLength": 36 },
					{ "buffer": 0, "byteOffset": 72, "byteLength": 24 },
					{ "buffer": 0, "byteOffset": 96, "byteLength": 6 }
				],
				"accessors": [
					{
						"bufferView": 0,
						"componentType": 5126,
						"count": 3,
						"type": "VEC3",
						"min": [0.0, 0.0, 0.0],
						"max": [1.0, 1.0, 0.0]
					},
					{ "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3" },
					{ "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2" },
					{ "bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR" }
				]
			})";

			FS::WriteText(path, source, true);
			return path;
		}
	}

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

	TEST(Mesh_LoadingSceneHierarchyAccumulatesMatrixAndTrsTransforms)
	{
		if (!Testing::AppHarness::IsAvailable())
			return;

		const Testing::TempDir tempDir;
		const Mesh mesh(WriteNestedTransformMesh(tempDir).string());

		REQUIRE CHECK_EQUAL(1u, static_cast<uint32_t>(mesh.GetSubmeshes().size()));

		const glm::mat4 rootTransform = glm::rotate(glm::mat4(1.0f), -glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
		const glm::mat4 nodeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 3.0f, 4.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 1.0f, 1.0f));
		const glm::mat4 expectedTransform = rootTransform * nodeTransform;
		const glm::mat4& actualTransform = mesh.GetSubmeshes().front().LocalTransform;
		for (int column = 0; column < 4; column++)
			CHECK_VEC4_CLOSE(expectedTransform[column], actualTransform[column], 0.0001f);
		CHECK_VEC3_CLOSE(glm::vec3(2.0f, 4.0f, -4.0f), mesh.GetBounds().Min, 0.0001f);
		CHECK_VEC3_CLOSE(glm::vec3(4.0f, 4.0f, -3.0f), mesh.GetBounds().Max, 0.0001f);
	}
}
