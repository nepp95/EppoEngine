#include "pch.h"
#include "Scene.h"

#include "Asset/AssetManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Entity.h"
#include "Scripting/ScriptEngine.h"

#include <bullet/btBulletDynamicsCommon.h>

namespace Eppo
{
	static btDefaultCollisionConfiguration* s_collisionConfig = new btDefaultCollisionConfiguration();
	static btCollisionDispatcher* s_collisionDispatcher = new btCollisionDispatcher(s_collisionConfig);
	static btBroadphaseInterface* s_broadPhaseInterface = new btDbvtBroadphase();
	static btSequentialImpulseConstraintSolver* s_Solver = new btSequentialImpulseConstraintSolver();

	namespace Utils
	{
		static glm::vec3 BulletToGlm(const btVector3& v)
		{
			return glm::vec3(v.getX(), v.getY(), v.getZ());
		}

		static glm::quat BulletToGlm(const btQuaternion& q)
		{
			return glm::quat(q.getW(), q.getX(), q.getY(), q.getZ());
		}

		static btVector3 GlmToBullet(const glm::vec3& v)
		{
			return btVector3(v.x, v.y, v.z);
		}

		static btQuaternion GlmToBullet(const glm::quat& q)
		{
			return btQuaternion(q.x, q.y, q.z, q.w);
		}
	}

	void Scene::SetViewportSize(uint32_t width, uint32_t height)
	{
		EPPO_PROFILE_FUNCTION("Scene::SetViewportSize");

		auto view = m_Registry.view<CameraComponent>();
		for (auto e : view)
		{
			auto& component = m_Registry.get<CameraComponent>(e);
			component.Camera.SetViewportSize(width, height);
		}
	}

	void Scene::OnUpdateRuntime(float timestep)
	{
		EPPO_PROFILE_FUNCTION("Scene::OnUpdateRuntime");

		// Scripts
		{
			auto view = m_Registry.view<ScriptComponent>();
			for (auto e : view)
			{
				Entity entity(e, this);
				ScriptEngine::OnUpdateEntity(entity, timestep);
			}
		}

		// Physics
		m_PhysicsWorld->stepSimulation(timestep, 10);

		auto view = m_Registry.view<RigidBodyComponent>();
		for (auto e : view)
		{
			Entity entity(e, this);
			auto& transform = entity.GetComponent<TransformComponent>();
			const auto& rigidbody = entity.GetComponent<RigidBodyComponent>();

			btRigidBody* body = rigidbody.RuntimeBody.GetBody();
			btTransform trans;

			if (body && body->getMotionState())
				body->getMotionState()->getWorldTransform(trans);

			const auto& position = trans.getOrigin();
			transform.Translation = Utils::BulletToGlm(position);

			trans.getRotation().getEulerZYX(transform.Rotation.z, transform.Rotation.y, transform.Rotation.x);
		}
	}

	void Scene::OnRenderEditor(const Ref<SceneRenderer>& sceneRenderer, const EditorCamera& editorCamera)
	{
		EPPO_PROFILE_FUNCTION("Scene::OnRenderEditor");

		sceneRenderer->BeginScene(editorCamera);

		RenderScene(sceneRenderer);

		sceneRenderer->EndScene();
	}

	void Scene::OnRenderRuntime(const Ref<SceneRenderer>& sceneRenderer)
	{
		EPPO_PROFILE_FUNCTION("Scene::OnRenderRuntime");

		SceneCamera* sceneCamera = nullptr;
		glm::mat4 cameraTransform;

		{
			auto view = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto e : view)
			{
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(e);
				sceneCamera = &camera.Camera;
				cameraTransform = transform.GetTransform();

				break;
			}
		}

		if (sceneCamera)
		{
			sceneRenderer->BeginScene(*sceneCamera, cameraTransform);

			RenderScene(sceneRenderer);
			
			sceneRenderer->EndScene();
		}
	}

	void Scene::OnRuntimeStart()
	{
		EPPO_PROFILE_FUNCTION("Scene::OnRuntimeStart");

		m_IsRunning = true;

		OnPhysicsStart();
		ScriptEngine::OnRuntimeStart();

		auto view = m_Registry.view<ScriptComponent>();
		for (auto e : view)
		{
			Entity entity(e, this);
			ScriptEngine::OnCreateEntity(entity);
		}
	}

	void Scene::OnRuntimeStop()
	{
		EPPO_PROFILE_FUNCTION("Scene::OnRuntimeStop");

		m_IsRunning = false;

		OnPhysicsStop();
		ScriptEngine::OnRuntimeStop();
	}

	Ref<Scene> Scene::Copy(Ref<Scene> scene)
	{
		EPPO_PROFILE_FUNCTION("Scene::Copy");

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
		CopyComponent<ScriptComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<RigidBodyComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<CameraComponent>(srcRegistry, dstRegistry, entityMap);

		return newScene;
	}

	template<typename T>
	void Scene::TryCopyComponent(Entity srcEntity, Entity dstEntity)
	{
		EPPO_PROFILE_FUNCTION("Scene::TryCopyComponent");

		if (srcEntity.HasComponent<T>())
			dstEntity.AddOrReplaceComponent<T>(srcEntity.GetComponent<T>());
	}

	template<typename T>
	void Scene::CopyComponent(entt::registry& srcRegistry, entt::registry& dstRegistry, const std::unordered_map<UUID, entt::entity>& entityMap)
	{
		EPPO_PROFILE_FUNCTION("Scene::CopyComponent");

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

		m_EntityMap[uuid] = entity;

		return entity;
	}

	Entity Scene::DuplicateEntity(Entity entity)
	{
		EPPO_PROFILE_FUNCTION("Scene::DuplicateEntity");

		std::string name = entity.GetName();
		Entity newEntity = CreateEntity(name);

		TryCopyComponent<TransformComponent>(entity, newEntity);
		TryCopyComponent<SpriteComponent>(entity, newEntity);
		TryCopyComponent<MeshComponent>(entity, newEntity);
		TryCopyComponent<DirectionalLightComponent>(entity, newEntity);
		TryCopyComponent<ScriptComponent>(entity, newEntity);
		TryCopyComponent<RigidBodyComponent>(entity, newEntity);
		TryCopyComponent<CameraComponent>(entity, newEntity);

		return newEntity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		EPPO_PROFILE_FUNCTION("Scene::DestroyEntity");

		m_EntityMap.erase(entity.GetUUID());
		m_Registry.destroy(entity);
	}

	Entity Scene::FindEntityByUUID(UUID uuid)
	{
		EPPO_PROFILE_FUNCTION("Scene::FindEntityByUUID");

		auto it = m_EntityMap.find(uuid);
		if (it != m_EntityMap.end())
			return { it->second, this };

		return {};
	}

	Entity Scene::FindEntityByName(std::string_view name)
	{
		EPPO_PROFILE_FUNCTION("Scene::FindEntityByName");

		auto view = m_Registry.view<TagComponent>();
		for (auto e : view)
		{
			const auto& tc = view.get<TagComponent>(e);
			if (tc.Tag == name)
				return { e, this };
		}

		return {};
	}

	void Scene::OnPhysicsStart()
	{
		EPPO_PROFILE_FUNCTION("Scene::OnPhysicsStart");

		m_PhysicsWorld = new btDiscreteDynamicsWorld(s_collisionDispatcher, s_broadPhaseInterface, s_Solver, s_collisionConfig);
		m_PhysicsWorld->setGravity(btVector3(0.0f, -9.81f, 0.0f));

		auto view = m_Registry.view<RigidBodyComponent>();
		for (auto e : view)
		{
			Entity entity(e, this);
			const auto& transform = entity.GetComponent<TransformComponent>();
			auto& rigidbody = entity.GetComponent<RigidBodyComponent>();

			btCollisionShape* shape = new btBoxShape(btVector3(transform.Scale.x, transform.Scale.y, transform.Scale.z));

			btTransform bTransform;
			bTransform.setIdentity();
			bTransform.setOrigin(btVector3(transform.Translation.x, transform.Translation.y, transform.Translation.z));
			bTransform.setRotation(Utils::GlmToBullet(glm::quat(transform.Rotation)));

			bool isDynamic = rigidbody.Type == RigidBodyComponent::BodyType::Dynamic;
			btScalar mass(0.0f);
			if (isDynamic)
				mass = rigidbody.Mass;

			btVector3 localInertia(btVector3(0.0f, 0.0f, 0.0f));
			if (isDynamic)
				shape->calculateLocalInertia(mass, localInertia);

			auto* motionState = new btDefaultMotionState(bTransform);
			btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, localInertia);
			auto* body = new btRigidBody(rbInfo);

			m_PhysicsWorld->addRigidBody(body);
			rigidbody.RuntimeBody = RigidBody(body);
		}
	}

	void Scene::OnPhysicsStop()
	{
		EPPO_PROFILE_FUNCTION("Scene::OnPhysicsStop");

		for (int i = m_PhysicsWorld->getNumCollisionObjects() - 1; i >= 0; i--)
		{
			btCollisionObject* obj = m_PhysicsWorld->getCollisionObjectArray()[i];
			btRigidBody* body = btRigidBody::upcast(obj);

			if (body && body->getMotionState())
				delete body->getMotionState();

			if (body && body->getCollisionShape())
				delete body->getCollisionShape();

			m_PhysicsWorld->removeCollisionObject(obj);
			delete obj;
		}

		auto view = m_Registry.view<RigidBodyComponent>();
		for (auto e : view)
		{
			Entity entity(e, this);
			auto& rigidbody = entity.GetComponent<RigidBodyComponent>();

			rigidbody.RuntimeBody.ClearBody();
		}

		delete m_PhysicsWorld;
		m_PhysicsWorld = nullptr;
	}

	void Scene::RenderScene(Ref<SceneRenderer> sceneRenderer)
	{
		EPPO_PROFILE_FUNCTION("Scene::RenderScene");

		{
			auto view = m_Registry.view<MeshComponent, TransformComponent>();

			for (const EntityHandle entity : view)
			{
				auto [meshC, transform] = view.get<MeshComponent, TransformComponent>(entity);
				if (meshC.MeshHandle)
				{
					Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(meshC.MeshHandle);

					if (mesh)
					{
						Ref<MeshCommand> meshCommand = CreateRef<MeshCommand>();
						meshCommand->Handle = entity;
						meshCommand->Mesh = mesh;
						meshCommand->Transform = transform.GetTransform();

						sceneRenderer->SubmitDrawCommand(EntityType::Mesh, meshCommand);
					}
				}
			}
		}

		{
			auto view = m_Registry.view<PointLightComponent, TransformComponent>();

			for (const EntityHandle entity : view)
			{
				auto [pl, transform] = view.get<PointLightComponent, TransformComponent>(entity);
				
				Ref<PointLightCommand> pointLightCommand = CreateRef<PointLightCommand>();
				pointLightCommand->Handle = entity;
				pointLightCommand->Position = transform.Translation;
				pointLightCommand->Color = pl.Color;

				sceneRenderer->SubmitDrawCommand(EntityType::PointLight, pointLightCommand);
			}
		}
	}
}
