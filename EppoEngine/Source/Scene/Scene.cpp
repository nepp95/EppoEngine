#include "pch.h"
#include "Scene.h"

#include "Asset/AssetManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Entity.h"

#include <bullet/btBulletDynamicsCommon.h>

namespace Eppo
{
	static btDefaultCollisionConfiguration* s_collisionConfig = new btDefaultCollisionConfiguration();
	static btCollisionDispatcher* s_collisionDispatcher = new btCollisionDispatcher(s_collisionConfig);
	static btBroadphaseInterface* s_broadPhaseInterface = new btDbvtBroadphase();
	static btSequentialImpulseConstraintSolver* s_Solver = new btSequentialImpulseConstraintSolver();

	void Scene::OnUpdateRuntime(float timestep)
	{
		EPPO_PROFILE_FUNCTION("Scene::OnUpdate");

		m_PhysicsWorld->stepSimulation(timestep, 10);

		auto view = m_Registry.view<RigidBodyComponent>();
		for (auto e : view)
		{
			Entity entity(e, this);
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& rigidbody = entity.GetComponent<RigidBodyComponent>();

			btRigidBody* body = (btRigidBody*)rigidbody.RuntimeBody;
			btTransform trans;

			if (body && body->getMotionState())
				body->getMotionState()->getWorldTransform(trans);

			const auto& position = trans.getOrigin();
			transform.Translation = glm::vec3(position.getX(), position.getY(), position.getZ());
		}
	}

	void Scene::RenderEditor(const Ref<SceneRenderer>& sceneRenderer, const EditorCamera& editorCamera)
	{
		sceneRenderer->BeginScene(editorCamera);

		{
			auto view = m_Registry.view<DirectionalLightComponent, TransformComponent>();

			for (const EntityHandle entity : view)
			{
				auto [dlc, tc] = view.get<DirectionalLightComponent, TransformComponent>(entity);
				sceneRenderer->SubmitDirectionalLight(dlc);
				break;
			}
		}

		{
			auto view = m_Registry.view<MeshComponent, TransformComponent>();

			for (const EntityHandle entity : view)
			{
				auto [meshC, transform] = view.get<MeshComponent, TransformComponent>(entity);
				if (meshC.MeshHandle)
				{
					Ref<Mesh> mesh = AssetManager::Get().GetAsset<Mesh>(meshC.MeshHandle);
					sceneRenderer->SubmitMesh(transform.GetTransform(), mesh, entity);
				}
			}
		}

		sceneRenderer->EndScene();
	}

	void Scene::OnRuntimeStart()
	{
		m_IsRunning = true;

		OnPhysicsStart();
	}

	void Scene::OnRuntimeStop()
	{
		m_IsRunning = false;

		OnPhysicsStop();
	}

	Ref<Scene> Scene::Copy(Ref<Scene> scene)
	{
		// Create a new scene and get a reference to both entity registries
		Ref<Scene> newScene = CreateRef<Scene>();

		auto& srcRegistry = scene->m_Registry;
		auto& dstRegistry = newScene->m_Registry;

		// Every entity in the scene has an ID component
		std::unordered_map<UUID, entt::entity> entityMap;
		auto idView = srcRegistry.view<IDComponent>();

		for (auto entity : idView)
		{
			UUID uuid = srcRegistry.get<IDComponent>(entity);
			const auto& name = srcRegistry.get<TagComponent>(entity);
			Entity newEntity = newScene->CreateEntityWithUUID(uuid, name);
			entityMap[uuid] = newEntity;
		}

		CopyComponent<TransformComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<SpriteComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<MeshComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<DirectionalLightComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<RigidBodyComponent>(srcRegistry, dstRegistry, entityMap);

		return newScene;
	}

	template<typename T>
	void Scene::TryCopyComponent(Entity srcEntity, Entity dstEntity)
	{
		if (srcEntity.HasComponent<T>())
			dstEntity.AddOrReplaceComponent<T>(srcEntity.GetComponent<T>());
	}

	template<typename T>
	void Scene::CopyComponent(entt::registry& srcRegistry, entt::registry& dstRegistry, const std::unordered_map<UUID, entt::entity>& entityMap)
	{
		auto view = srcRegistry.view<T>();

		for (auto srcEntity : view)
		{
			entt::entity dstEntity = entityMap.at(srcRegistry.get<IDComponent>(srcEntity).ID);
			auto& srcComponent = srcRegistry.get<T>(srcEntity);
			dstRegistry.emplace_or_replace<T>(dstEntity, srcComponent);
		}
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		EPPO_PROFILE_FUNCTION("Scene::CreateEntity");

		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		EPPO_PROFILE_FUNCTION("Scene::CreateEntityWithUUID");

		Entity entity(m_Registry.create(), this);

		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();

		auto& tag = entity.AddComponent<TagComponent>();
		tag = name.empty() ? "Entity" : name;

		return entity;
	}

	Entity Scene::DuplicateEntity(Entity entity)
	{
		std::string name = entity.GetName();
		Entity newEntity = CreateEntity(name);

		TryCopyComponent<TransformComponent>(entity, newEntity);
		TryCopyComponent<SpriteComponent>(entity, newEntity);
		TryCopyComponent<MeshComponent>(entity, newEntity);
		TryCopyComponent<DirectionalLightComponent>(entity, newEntity);
		TryCopyComponent<RigidBodyComponent>(entity, newEntity);

		return newEntity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		EPPO_PROFILE_FUNCTION("Scene::DestroyEntity");

		m_Registry.destroy(entity);
	}

	void Scene::OnPhysicsStart()
	{
		m_PhysicsWorld = new btDiscreteDynamicsWorld(s_collisionDispatcher, s_broadPhaseInterface, s_Solver, s_collisionConfig);
		m_PhysicsWorld->setGravity(btVector3(0.0f, -9.81f, 0.0f));

		btAlignedObjectArray<btCollisionShape*> collisionShapes;

		auto view = m_Registry.view<RigidBodyComponent>();
		for (auto e : view)
		{
			Entity entity(e, this);
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& rigidbody = entity.GetComponent<RigidBodyComponent>();

			btCollisionShape* shape = new btBoxShape(btVector3(transform.Scale.x / 2.0f, transform.Scale.y / 2.0f, transform.Scale.z / 2.0f));
			collisionShapes.push_back(shape);

			btTransform bTransform;
			bTransform.setIdentity();
			bTransform.setOrigin(btVector3(transform.Translation.x, transform.Translation.y, transform.Translation.z));

			btScalar mass(0.0f);
			if (rigidbody.Type == RigidBodyComponent::BodyType::Dynamic)
				mass = 1.0f;

			bool isDynamic = (mass != 0.0f);

			btVector3 localInertia(btVector3(0.0f, 0.0f, 0.0f));
			if (isDynamic)
				shape->calculateLocalInertia(mass, localInertia);

			btDefaultMotionState* motionState = new btDefaultMotionState(bTransform);
			btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, localInertia);
			btRigidBody* body = new btRigidBody(rbInfo);

			m_PhysicsWorld->addRigidBody(body);
			rigidbody.RuntimeBody = body;
		}
	}

	void Scene::OnPhysicsStop()
	{
		delete m_PhysicsWorld;
		m_PhysicsWorld = nullptr;
	}
}
