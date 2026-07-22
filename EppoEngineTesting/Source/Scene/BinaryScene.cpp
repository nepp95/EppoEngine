#include "Support/EppoTest.h"

#include "Asset/PackFormat.h"
#include "Core/BufferReader.h"
#include "Core/BufferWriter.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

using namespace Eppo;

SUITE(Scene)
{
    namespace
    {
        auto SerializeBinary(const Ref<Scene>& scene, Buffer& buffer) -> bool
        {
            BufferWriter sizingWriter;
            if (!SceneSerializer(scene).Serialize(sizingWriter))
                return false;

            buffer.Allocate(sizingWriter.GetSize());
            BufferWriter writer(buffer);
            return SceneSerializer(scene).Serialize(writer) && writer.GetOffset() == buffer.Size;
        }

        auto DeserializeBinary(const Ref<Scene>& scene, const Buffer& buffer) -> bool
        {
            BufferReader reader(buffer);
            return SceneSerializer(scene).Deserialize(reader);
        }
    }

    TEST(SceneSerializer_Binary_EmptySceneEnvironmentAndHandleRoundTrip)
    {
        const Ref<Scene> source = CreateRef<Scene>();
        source->Handle = AssetHandle(7001);
        source->GetEnvironment() = {
            .SkyboxHandle = AssetHandle(81),
            .ZenithColor = { 0.1f, 0.2f, 0.3f },
            .HorizonColor = { 0.4f, 0.5f, 0.6f },
            .GroundColor = { 0.7f, 0.8f, 0.9f },
            .AmbientIntensity = 2.5f,
        };

        Buffer buffer;
        REQUIRE CHECK(SerializeBinary(source, buffer));

        const Ref<Scene> loaded = CreateRef<Scene>();
        loaded->Handle = source->Handle;
        REQUIRE CHECK(DeserializeBinary(loaded, buffer));

        const auto& environment = loaded->GetEnvironment();
        CHECK_EQUAL(static_cast<uint64_t>(source->Handle), static_cast<uint64_t>(loaded->Handle));
        CHECK_EQUAL(81, static_cast<uint64_t>(environment.SkyboxHandle));
        CHECK_CLOSE(0.1f, environment.ZenithColor.x, 0.0001f);
        CHECK_CLOSE(0.5f, environment.HorizonColor.y, 0.0001f);
        CHECK_CLOSE(0.9f, environment.GroundColor.z, 0.0001f);
        CHECK_CLOSE(2.5f, environment.AmbientIntensity, 0.0001f);

        uint32_t entityCount = 0;
        loaded->ForEachEntity([&](Entity) { entityCount++; });
        CHECK_EQUAL(0, entityCount);

        buffer.Release();
    }

    TEST(SceneSerializer_Binary_EntitiesAndAllNonScriptComponentsRoundTrip)
    {
        const Ref<Scene> source = CreateRef<Scene>();
        source->Handle = AssetHandle(7002);

        Entity parent = source->CreateEntityWithUUID(UUID(100), "Parent");
        auto& camera = parent.AddComponent<CameraComponent>();
        camera.Primary = false;
        camera.Camera.SetPerspective(58.0f, 0.25f, 850.0f);

        Entity child = source->CreateEntityWithUUID(UUID(200), "Child");
        child.GetComponent<TransformComponent>().Translation = { 1.0f, 2.0f, 3.0f };
        child.GetComponent<TransformComponent>().Rotation = { 0.2f, 0.3f, 0.4f };
        child.GetComponent<TransformComponent>().Scale = { 2.0f, 3.0f, 4.0f };
        child.AddComponent<MeshComponent>(AssetHandle(42));
        auto& light = child.AddComponent<PointLightComponent>();
        light.Color = { 0.3f, 0.4f, 0.5f };
        light.Intensity = 17.0f;
        auto& rigidBody = child.AddComponent<RigidBodyComponent>();
        rigidBody.Type = RigidBodyComponent::BodyType::Dynamic;
        rigidBody.GravityScale = 2.0f;
        rigidBody.LinearDamping = 0.25f;
        rigidBody.AngularDamping = 0.5f;
        auto& box = child.AddComponent<BoxColliderComponent>();
        box.HalfSize = { 1.0f, 2.0f, 3.0f };
        box.Offset = { 0.1f, 0.2f, 0.3f };
        auto& sphere = child.AddComponent<SphereColliderComponent>();
        sphere.Radius = 4.0f;
        sphere.Offset = { 0.4f, 0.5f, 0.6f };
        auto& capsule = child.AddComponent<CapsuleColliderComponent>();
        capsule.Radius = 1.5f;
        capsule.Height = 6.0f;
        capsule.Offset = { 0.7f, 0.8f, 0.9f };
        auto& cylinder = child.AddComponent<CylinderColliderComponent>();
        cylinder.Radius = 2.5f;
        cylinder.Height = 8.0f;
        cylinder.Offset = { 1.1f, 1.2f, 1.3f };
        source->SetParent(child, parent);

        Buffer buffer;
        REQUIRE CHECK(SerializeBinary(source, buffer));

        const Ref<Scene> loaded = CreateRef<Scene>();
        loaded->Handle = source->Handle;
        REQUIRE CHECK(DeserializeBinary(loaded, buffer));

        const Entity loadedParent = loaded->GetEntityByUUID(UUID(100));
        const Entity loadedChild = loaded->GetEntityByUUID(UUID(200));
        REQUIRE CHECK(loadedParent);
        REQUIRE CHECK(loadedChild);
        CHECK_EQUAL(std::string("Parent"), loadedParent.GetName());
        CHECK_EQUAL(std::string("Child"), loadedChild.GetName());

        const auto& loadedCamera = loadedParent.GetComponent<CameraComponent>();
        CHECK(!loadedCamera.Primary);
        CHECK_CLOSE(58.0f, loadedCamera.Camera.GetPerspectiveVerticalFov(), 0.0001f);
        CHECK_CLOSE(0.25f, loadedCamera.Camera.GetPerspectiveNearClip(), 0.0001f);
        CHECK_CLOSE(850.0f, loadedCamera.Camera.GetPerspectiveFarClip(), 0.0001f);

        const auto& transform = loadedChild.GetComponent<TransformComponent>();
        CHECK_CLOSE(1.0f, transform.Translation.x, 0.0001f);
        CHECK_CLOSE(0.3f, transform.Rotation.y, 0.0001f);
        CHECK_CLOSE(4.0f, transform.Scale.z, 0.0001f);
        CHECK_EQUAL(42, static_cast<uint64_t>(loadedChild.GetComponent<MeshComponent>().MeshHandle));
        CHECK_CLOSE(0.4f, loadedChild.GetComponent<PointLightComponent>().Color.y, 0.0001f);
        CHECK_CLOSE(17.0f, loadedChild.GetComponent<PointLightComponent>().Intensity, 0.0001f);

        const auto& loadedRigidBody = loadedChild.GetComponent<RigidBodyComponent>();
        CHECK(loadedRigidBody.Type == RigidBodyComponent::BodyType::Dynamic);
        CHECK_CLOSE(2.0f, loadedRigidBody.GravityScale, 0.0001f);
        CHECK_CLOSE(0.25f, loadedRigidBody.LinearDamping, 0.0001f);
        CHECK_CLOSE(0.5f, loadedRigidBody.AngularDamping, 0.0001f);
        CHECK_CLOSE(3.0f, loadedChild.GetComponent<BoxColliderComponent>().HalfSize.z, 0.0001f);
        CHECK_CLOSE(4.0f, loadedChild.GetComponent<SphereColliderComponent>().Radius, 0.0001f);
        CHECK_CLOSE(6.0f, loadedChild.GetComponent<CapsuleColliderComponent>().Height, 0.0001f);
        CHECK_CLOSE(8.0f, loadedChild.GetComponent<CylinderColliderComponent>().Height, 0.0001f);

        REQUIRE CHECK(loadedParent.HasComponent<RelationshipComponent>());
        REQUIRE CHECK(loadedChild.HasComponent<RelationshipComponent>());
        CHECK_EQUAL(1, loadedParent.GetComponent<RelationshipComponent>().Children.size());
        CHECK_EQUAL(100, static_cast<uint64_t>(loadedChild.GetComponent<RelationshipComponent>().Parent));

        buffer.Release();
    }

    TEST(SceneSerializer_Binary_EntityOrderIsDeterministic)
    {
        const Ref<Scene> first = CreateRef<Scene>();
        const Ref<Scene> second = CreateRef<Scene>();
        first->Handle = AssetHandle(7003);
        second->Handle = first->Handle;

        first->CreateEntityWithUUID(UUID(2), "Second");
        first->CreateEntityWithUUID(UUID(1), "First");
        second->CreateEntityWithUUID(UUID(1), "First");
        second->CreateEntityWithUUID(UUID(2), "Second");

        Buffer firstBuffer;
        Buffer secondBuffer;
        REQUIRE CHECK(SerializeBinary(first, firstBuffer));
        REQUIRE CHECK(SerializeBinary(second, secondBuffer));
        CHECK_EQUAL(firstBuffer.Size, secondBuffer.Size);
        CHECK_ARRAY_EQUAL(firstBuffer.Data, secondBuffer.Data, firstBuffer.Size);

        firstBuffer.Release();
        secondBuffer.Release();
    }

    TEST(SceneSerializer_Binary_ReusesRelationshipRepair)
    {
        const Ref<Scene> source = CreateRef<Scene>();
        source->Handle = AssetHandle(7004);
        source->CreateEntityWithUUID(UUID(10), "Parent");
        Entity child = source->CreateEntityWithUUID(UUID(20), "Child");
        child.AddComponent<RelationshipComponent>().Parent = UUID(10);

        Buffer buffer;
        REQUIRE CHECK(SerializeBinary(source, buffer));

        const Ref<Scene> loaded = CreateRef<Scene>();
        loaded->Handle = source->Handle;
        REQUIRE CHECK(DeserializeBinary(loaded, buffer));
        CHECK(!loaded->GetEntityByUUID(UUID(20)).HasComponent<RelationshipComponent>());

        buffer.Release();
    }

    TEST(SceneSerializer_Binary_RejectsHandleMismatchUnknownMaskAndTruncation)
    {
        const Ref<Scene> source = CreateRef<Scene>();
        source->Handle = AssetHandle(7005);
        source->CreateEntityWithUUID(UUID(1), "Entity").AddComponent<MeshComponent>(AssetHandle(9));

        Buffer buffer;
        REQUIRE CHECK(SerializeBinary(source, buffer));

        uint32_t serializedVersion = PackFormat::Scene.Version + 1;
        std::memcpy(buffer.Data + sizeof(uint32_t), &serializedVersion, sizeof(serializedVersion));
        const Ref<Scene> wrongVersion = CreateRef<Scene>();
        wrongVersion->Handle = source->Handle;
        CHECK(!DeserializeBinary(wrongVersion, buffer));
        serializedVersion = PackFormat::Scene.Version;
        std::memcpy(buffer.Data + sizeof(uint32_t), &serializedVersion, sizeof(serializedVersion));

        const Ref<Scene> wrongHandle = CreateRef<Scene>();
        wrongHandle->Handle = AssetHandle(9999);
        CHECK(!DeserializeBinary(wrongHandle, buffer));

        Buffer truncated(buffer.Data, buffer.Size - 1);
        const Ref<Scene> truncatedScene = CreateRef<Scene>();
        truncatedScene->Handle = source->Handle;
        CHECK(!DeserializeBinary(truncatedScene, truncated));

        BufferWriter sizingWriter;
        const EnvironmentSettings environment{};
        const TransformComponent transform{};
        CHECK(sizingWriter.Write(PackFormat::Scene.Magic));
        CHECK(sizingWriter.Write(PackFormat::Scene.Version));
        CHECK(sizingWriter.Write(uint64_t{ 7005 }));
        CHECK(sizingWriter.Write(environment));
        CHECK(sizingWriter.Write(uint32_t{ 1 }));
        CHECK(sizingWriter.Write(uint64_t{ 1 }));
        CHECK(sizingWriter.WriteString("Entity"));
        CHECK(sizingWriter.Write(transform));
        CHECK(sizingWriter.Write(uint16_t{ 1u << 10 }));
        Buffer unknownMaskBuffer(sizingWriter.GetSize());
        BufferWriter writer(unknownMaskBuffer);
        CHECK(writer.Write(PackFormat::Scene.Magic));
        CHECK(writer.Write(PackFormat::Scene.Version));
        CHECK(writer.Write(uint64_t{ 7005 }));
        CHECK(writer.Write(environment));
        CHECK(writer.Write(uint32_t{ 1 }));
        CHECK(writer.Write(uint64_t{ 1 }));
        CHECK(writer.WriteString("Entity"));
        CHECK(writer.Write(transform));
        CHECK(writer.Write(uint16_t{ 1u << 10 }));
        const Ref<Scene> unknownMaskScene = CreateRef<Scene>();
        unknownMaskScene->Handle = source->Handle;
        CHECK(!DeserializeBinary(unknownMaskScene, unknownMaskBuffer));

        buffer.Release();
        unknownMaskBuffer.Release();
    }

    TEST(SceneSerializer_Binary_RejectsImpossibleEntityCountAndTrailingBytes)
    {
        const EnvironmentSettings environment{};
        BufferWriter sizingWriter;
        CHECK(sizingWriter.Write(PackFormat::Scene.Magic));
        CHECK(sizingWriter.Write(PackFormat::Scene.Version));
        CHECK(sizingWriter.Write(uint64_t{ 7006 }));
        CHECK(sizingWriter.Write(environment));
        CHECK(sizingWriter.Write(std::numeric_limits<uint32_t>::max()));
        Buffer countBuffer(sizingWriter.GetSize());
        BufferWriter countWriter(countBuffer);
        CHECK(countWriter.Write(PackFormat::Scene.Magic));
        CHECK(countWriter.Write(PackFormat::Scene.Version));
        CHECK(countWriter.Write(uint64_t{ 7006 }));
        CHECK(countWriter.Write(environment));
        CHECK(countWriter.Write(std::numeric_limits<uint32_t>::max()));

        const Ref<Scene> countScene = CreateRef<Scene>();
        countScene->Handle = AssetHandle(7006);
        CHECK(!DeserializeBinary(countScene, countBuffer));

        const Ref<Scene> source = CreateRef<Scene>();
        source->Handle = AssetHandle(7006);
        Buffer validBuffer;
        REQUIRE CHECK(SerializeBinary(source, validBuffer));
        Buffer trailingBuffer(validBuffer.Size + 1);
        std::memcpy(trailingBuffer.Data, validBuffer.Data, validBuffer.Size);
        trailingBuffer.Data[validBuffer.Size] = 0xff;
        const Ref<Scene> trailingScene = CreateRef<Scene>();
        trailingScene->Handle = source->Handle;
        CHECK(!DeserializeBinary(trailingScene, trailingBuffer));

        countBuffer.Release();
        validBuffer.Release();
        trailingBuffer.Release();
    }
}
