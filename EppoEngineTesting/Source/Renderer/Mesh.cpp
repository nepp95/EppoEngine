#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"
#include "TestSupport/GlmCheck.h"
#include "TestSupport/TempDir.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Mesh.h"
#include "Renderer/Vertex.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <thread>

using namespace Eppo;

namespace
{
    enum class TangentFixture
    {
        Imported,
        Generated,
        DegenerateTextureCoordinates,
    };

    auto WriteTangentMesh(const Testing::TempDir& tempDir, const TangentFixture fixture) -> std::filesystem::path
    {
        const bool hasTangents = fixture == TangentFixture::Imported;
        const std::string name = hasTangents ? "ImportedTangents.gltf"
            : fixture == TangentFixture::Generated ? "GeneratedTangents.gltf" : "DegenerateTextureCoordinates.gltf";
        const std::filesystem::path path = tempDir.File(name);

        const std::string data = hasTangents
            ? "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/"
              "AAAAAAAAAAAAAIA/AAAAAAAAgD8AAAAAAACAvwAAAAAAAIA/AAAAAAAAgL8AAAAAAACAPwAAAAAAAIC/AAABAAIA"
            : fixture == TangentFixture::Generated
                ? "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/"
                  "AAAAAAAAAAAAAIA/AAABAAIA"
                : "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAA"
                  "AAAAAAAAAAAAAAAAAAABAAIA";
        const std::string tangentAttribute = hasTangents ? R"(, "TANGENT": 3)" : "";
        const uint32_t indexAccessor = hasTangents ? 4u : 3u;
        const uint32_t byteLength = hasTangents ? 150u : 102u;
        const std::string bufferViews = hasTangents
            ? R"([
                { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
                { "buffer": 0, "byteOffset": 36, "byteLength": 36 },
                { "buffer": 0, "byteOffset": 72, "byteLength": 24 },
                { "buffer": 0, "byteOffset": 96, "byteLength": 48 },
                { "buffer": 0, "byteOffset": 144, "byteLength": 6 }
            ])"
            : R"([
                { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
                { "buffer": 0, "byteOffset": 36, "byteLength": 36 },
                { "buffer": 0, "byteOffset": 72, "byteLength": 24 },
                { "buffer": 0, "byteOffset": 96, "byteLength": 6 }
            ])";
        const std::string accessors = hasTangents
            ? R"([
                { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0] },
                { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3" },
                { "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2" },
                { "bufferView": 3, "componentType": 5126, "count": 3, "type": "VEC4" },
                { "bufferView": 4, "componentType": 5123, "count": 3, "type": "SCALAR" }
            ])"
            : R"([
                { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0] },
                { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3" },
                { "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2" },
                { "bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR" }
            ])";

        const std::string source = std::string(R"({
            "asset": { "version": "2.0" },
            "scene": 0,
            "scenes": [{ "nodes": [0] }],
            "nodes": [{ "mesh": 0 }],
            "meshes": [{
                "name": "Triangle",
                "primitives": [{
                    "attributes": { "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2)")
            + tangentAttribute + R"( },
                    "indices": )" + std::to_string(indexAccessor) + R"(
                }]
            }],
            "buffers": [{
                "byteLength": )" + std::to_string(byteLength) + R"(,
                "uri": "data:application/octet-stream;base64,)" + data + R"("
            }],
            "bufferViews": )" + bufferViews + R"(,
            "accessors": )" + accessors + R"(
        })";

        EP_REQUIRE(FS::WriteText(path, source, true));
        return path;
    }

    auto ReadVertices(const Ref<VertexBuffer>& vertexBuffer) -> std::vector<Vertex>
    {
        const auto device = DeviceManager::Get()->GetDevice();
        const uint64_t size = vertexBuffer->GetSize();
        const auto readbackBuffer = device->createBuffer(nvrhi::BufferDesc{
            .byteSize = size,
            .debugName = "Mesh tangent test readback",
            .initialState = nvrhi::ResourceStates::CopyDest,
            .keepInitialState = true,
            .cpuAccess = nvrhi::CpuAccessMode::Read,
        });
        const auto commandList = device->createCommandList();

        commandList->open();
        commandList->copyBuffer(readbackBuffer, 0, vertexBuffer->GetBuffer(), 0, size);
        commandList->close();
        device->executeCommandList(commandList);
        EP_ASSERT(device->waitForIdle(), "Failed waiting for tangent test readback");

        const auto* data = static_cast<const Vertex*>(device->mapBuffer(readbackBuffer, nvrhi::CpuAccessMode::Read));
        EP_ASSERT(data, "Failed mapping tangent test readback");

        const uint64_t vertexCount = size / sizeof(Vertex);
        std::vector<Vertex> vertices(data, data + vertexCount);
        device->unmapBuffer(readbackBuffer);
        return vertices;
    }

    auto LoadFixtureVertices(const TangentFixture fixture) -> std::vector<Vertex>
    {
        const Testing::TempDir tempDir;
        const Mesh mesh(WriteTangentMesh(tempDir, fixture).string());

        EP_REQUIRE(mesh.IsValid());
        EP_REQUIRE_EQ(1u, static_cast<uint32_t>(mesh.GetSubmeshes().size()));
        return ReadVertices(mesh.GetSubmeshes().front().VertexBuffer);
    }

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

    auto WriteTexturedMesh(const Testing::TempDir& tempDir) -> std::filesystem::path
    {
        const std::filesystem::path path = tempDir.File("Textured.gltf");
        const std::filesystem::path bufferPath = tempDir.File("Textured.bin");

        const std::array positions{
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
        };
        const std::array normals{
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
        };
        const std::array textureCoordinates{
            glm::vec2(0.0f, 0.0f),
            glm::vec2(1.0f, 0.0f),
            glm::vec2(0.0f, 1.0f),
        };
        const std::array<uint16_t, 3> indices{ 0, 1, 2 };

        std::vector<char> bytes(102);
        std::memcpy(bytes.data(), positions.data(), sizeof(positions));
        std::memcpy(bytes.data() + 36, normals.data(), sizeof(normals));
        std::memcpy(bytes.data() + 72, textureCoordinates.data(), sizeof(textureCoordinates));
        std::memcpy(bytes.data() + 96, indices.data(), sizeof(indices));

        std::vector<char> imageBytes(18, 0);
        imageBytes[2] = 2;
        imageBytes[12] = 2;
        imageBytes[14] = 2;
        imageBytes[16] = 24;
        for (uint32_t i = 0; i < 4; i++)
            imageBytes.insert(imageBytes.end(), { static_cast<char>(64), static_cast<char>(128), static_cast<char>(200) });
        bytes.insert(bytes.end(), imageBytes.begin(), imageBytes.end());

        EP_REQUIRE(FS::WriteBytes(bufferPath, bytes, true));
        const std::string source = R"({
            "asset": { "version": "2.0" },
            "scene": 0,
            "scenes": [{ "nodes": [0] }],
            "nodes": [{ "mesh": 0 }],
            "meshes": [{
                "name": "Textured Triangle",
                "primitives": [{
                    "attributes": { "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 },
                    "indices": 3,
                    "material": 0
                }]
            }],
            "materials": [{
                "pbrMetallicRoughness": { "baseColorTexture": { "index": 0 } }
            }],
            "textures": [{ "source": 0 }],
            "images": [{ "bufferView": 4, "mimeType": "image/x-tga" }],
            "buffers": [{ "byteLength": 132, "uri": "Textured.bin" }],
            "bufferViews": [
                { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
                { "buffer": 0, "byteOffset": 36, "byteLength": 36 },
                { "buffer": 0, "byteOffset": 72, "byteLength": 24 },
                { "buffer": 0, "byteOffset": 96, "byteLength": 6 },
                { "buffer": 0, "byteOffset": 102, "byteLength": 30 }
            ],
            "accessors": [
                { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0] },
                { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3" },
                { "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2" },
                { "bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR" }
            ]
        })";

        EP_REQUIRE(FS::WriteText(path, source, true));
        return path;
    }

    auto WriteMaterialMesh(const Testing::TempDir& tempDir) -> std::filesystem::path
    {
        const std::filesystem::path path = tempDir.File("Materials.gltf");
        const std::string source = R"({
            "asset": { "version": "2.0" },
            "scene": 0,
            "scenes": [{ "nodes": [0] }],
            "nodes": [{ "mesh": 0 }],
            "meshes": [{
                "name": "Material Triangles",
                "primitives": [
                    { "attributes": { "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 }, "indices": 3, "material": 0 },
                    { "attributes": { "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 }, "indices": 3, "material": 1 },
                    { "attributes": { "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 }, "indices": 3, "material": 2 }
                ]
            }],
            "materials": [
                { "name": "Opaque", "alphaMode": "OPAQUE" },
                { "name": "Mask", "alphaMode": "MASK", "alphaCutoff": 0.37, "doubleSided": true },
                { "name": "Blend", "alphaMode": "BLEND", "alphaCutoff": 0.21 }
            ],
            "buffers": [{
                "byteLength": 102,
                "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAABAAIA"
            }],
            "bufferViews": [
                { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
                { "buffer": 0, "byteOffset": 36, "byteLength": 36 },
                { "buffer": 0, "byteOffset": 72, "byteLength": 24 },
                { "buffer": 0, "byteOffset": 96, "byteLength": 6 }
            ],
            "accessors": [
                { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0] },
                { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3" },
                { "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2" },
                { "bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR" }
            ]
        })";

        EP_REQUIRE(FS::WriteText(path, source, true));
        return path;
    }
}

TEST(Renderer, Material_DefaultTextureHandlesProduceInvalidIndices)
{
	const Material material;

	EXPECT_EQ(-1, material.GetDiffuseMapIndex());
	EXPECT_EQ(-1, material.GetNormalMapIndex());
	EXPECT_EQ(-1, material.GetRoughMetMapIndex());
}

TEST(Renderer, Material_SharedTextureHandleProducesSameGlobalIndex)
{
	const auto handle = CreateRef<BindlessHandle>();
	handle->Index = 42;

	Material first;
	first.DiffuseMap = handle;
	Material second;
	second.DiffuseMap = handle;

	EXPECT_EQ(42, first.GetDiffuseMapIndex());
	EXPECT_EQ(42, second.GetDiffuseMapIndex());
}

TEST(Renderer, Mesh_ImportedTangentsArePreserved)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const std::vector<Vertex> vertices = LoadFixtureVertices(TangentFixture::Imported);
    EP_REQUIRE_EQ(3u, static_cast<uint32_t>(vertices.size()));

    for (const Vertex& vertex : vertices)
        CHECK_VEC4_CLOSE(glm::vec4(0.0f, 1.0f, 0.0f, -1.0f), vertex.Tangent, 0.0001f);
}

TEST(Renderer, Mesh_MissingTangentsAreGenerated)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const std::vector<Vertex> vertices = LoadFixtureVertices(TangentFixture::Generated);
    EP_REQUIRE_EQ(3u, static_cast<uint32_t>(vertices.size()));

    for (const Vertex& vertex : vertices)
    {
        const glm::vec3 tangent = glm::vec3(vertex.Tangent);
        EXPECT_TRUE(std::isfinite(vertex.Tangent.x));
        EXPECT_TRUE(std::isfinite(vertex.Tangent.y));
        EXPECT_TRUE(std::isfinite(vertex.Tangent.z));
        EXPECT_TRUE(std::isfinite(vertex.Tangent.w));
        EXPECT_NEAR(1.0f, glm::length(tangent), 0.0001f);
        EXPECT_NEAR(0.0f, glm::dot(vertex.Normal, tangent), 0.0001f);
        CHECK_VEC4_CLOSE(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), vertex.Tangent, 0.0001f);
    }
}

TEST(Renderer, Mesh_DegenerateTextureCoordinatesReceiveFallbackTangents)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const std::vector<Vertex> vertices = LoadFixtureVertices(TangentFixture::DegenerateTextureCoordinates);
    EP_REQUIRE_EQ(3u, static_cast<uint32_t>(vertices.size()));

    for (const Vertex& vertex : vertices)
        CHECK_VEC4_CLOSE(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), vertex.Tangent, 0.0001f);
}

TEST(Renderer, Mesh_LoadingSceneHierarchyAccumulatesMatrixAndTrsTransforms)
{
	if (!Testing::AppHarness::IsAvailable())
		return;

	const Testing::TempDir tempDir;
	const Mesh mesh(WriteNestedTransformMesh(tempDir).string());

	EP_REQUIRE_EQ(1u, static_cast<uint32_t>(mesh.GetSubmeshes().size()));

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

TEST(Renderer, Mesh_EmbeddedImageLoadsAndAssignsMaterialHandle)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const Testing::TempDir tempDir;
    const Mesh mesh(WriteTexturedMesh(tempDir).string());

    EP_REQUIRE(mesh.IsValid());
    EP_REQUIRE_EQ(1u, static_cast<uint32_t>(mesh.GetImages().size()));
    const Ref<Image>& image = mesh.GetImage(0);
    EP_REQUIRE(image != nullptr);
    EXPECT_FALSE(image->IsLoaded.load(std::memory_order_acquire));

    for (uint32_t frame = 0; frame < 120 && !image->IsLoaded.load(std::memory_order_acquire); frame++)
    {
        Testing::AppHarness::AdvanceFrames(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EP_REQUIRE(image->IsLoaded.load(std::memory_order_acquire));
    EXPECT_EQ(2u, image->GetWidth());
    EXPECT_EQ(2u, image->GetHeight());
    EXPECT_TRUE(image->GetTexture() != nullptr);
    EXPECT_NE(-1, mesh.GetMaterial(0)->GetDiffuseMapIndex());
}

TEST(Renderer, Mesh_GltfMaterial_ImportsAlphaModeCutoffAndDoubleSided)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const Testing::TempDir tempDir;
    const Mesh mesh(WriteMaterialMesh(tempDir).string());

    EP_REQUIRE(mesh.IsValid());
    EP_REQUIRE_EQ(3u, static_cast<uint32_t>(mesh.GetSubmeshes().front().Primitives.size()));

    const Ref<Material>& opaque = mesh.GetMaterial(0);
    const Ref<Material>& mask = mesh.GetMaterial(1);
    const Ref<Material>& blend = mesh.GetMaterial(2);
    EP_REQUIRE(opaque != nullptr);
    EP_REQUIRE(mask != nullptr);
    EP_REQUIRE(blend != nullptr);

    EXPECT_EQ(MaterialAlphaMode::Opaque, opaque->AlphaMode);
    EXPECT_FALSE(opaque->DoubleSided);
    EXPECT_EQ(MaterialAlphaMode::Mask, mask->AlphaMode);
    EXPECT_NEAR(0.37f, mask->AlphaCutoff, 0.0001f);
    EXPECT_TRUE(mask->DoubleSided);
    EXPECT_EQ(MaterialAlphaMode::Blend, blend->AlphaMode);
    EXPECT_NEAR(0.21f, blend->AlphaCutoff, 0.0001f);
    EXPECT_FALSE(blend->DoubleSided);
}
