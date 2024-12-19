#pragma once

#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <entt/entt.hpp>

namespace Eppo
{
	using EntityHandle = entt::entity;

	class Entity
	{
	public:
		Entity() = default;
		Entity(EntityHandle entityHandle, Scene* scene);
		Entity(const Entity& other) = default;

		template<typename T>
		[[nodiscard]] bool HasComponent() const
		{
			return m_Scene->m_Registry.all_of<T>(m_EntityHandle);
		}

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args)
		{
			EPPO_ASSERT(!HasComponent<T>());
			T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
			return component;
		}

		template<typename T, typename... Args>
		T& AddOrReplaceComponent(Args&&... args)
		{
			T& component = m_Scene->m_Registry.emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
			return component;
		}

		template<typename T>
		void RemoveComponent() const
		{
			EPPO_ASSERT(HasComponent<T>());
			m_Scene->m_Registry.remove<T>(m_EntityHandle);
		}

		template<typename T>
		T& GetComponent()
		{
			EPPO_ASSERT(HasComponent<T>())
			return m_Scene->m_Registry.get<T>(m_EntityHandle);
		}

		explicit operator bool() const { return m_EntityHandle != entt::null; }
		explicit operator EntityHandle() const { return m_EntityHandle; }

		bool operator==(const Entity& other) const
		{
			return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
		}

		bool operator!=(const Entity& other) const
		{
			return !(*this == other);
		}
		
		const std::string& GetName() { return GetComponent<TagComponent>().Tag; }
		const UUID& GetUUID() { return GetComponent<IDComponent>().ID; }

	private:
		EntityHandle m_EntityHandle = entt::null;
		Scene* m_Scene;
	};
}
