#pragma once

#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <entt/entt.hpp>

namespace Eppo
{
    class Entity
    {
    public:
        Entity() = default;
        Entity(EntityHandle entityHandle, Scene* scene);

        template<typename T>
        [[nodiscard]] auto HasComponent() const -> bool
        {
            return m_Scene->m_Registry.all_of<T>(m_EntityHandle);
        }

        template<typename T, typename... Args>
        auto AddComponent(Args&&... args) -> T&
        {
            EP_ASSERT(!HasComponent<T>(), "Entity already has component!");
            T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
            return component;
        }

        template<typename T, typename... Args>
        auto TryAddComponent(Args&&... args) -> T&
        {
            if (HasComponent<T>())
                return GetComponent<T>();
            T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
            return component;
        }

        template<typename T, typename... Args>
        auto AddOrReplaceComponent(Args&&... args) -> T&
        {
            T& component = m_Scene->m_Registry.emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
            return component;
        }

        template<typename T>
        auto RemoveComponent() const -> bool
        {
            if (!HasComponent<T>())
                return false;

            m_Scene->m_Registry.remove<T>(m_EntityHandle);
            return true;
        }

        template<typename T>
        [[nodiscard]] auto GetComponent() const -> T&
        {
            EP_ASSERT(HasComponent<T>(), "Entity does not have component!");
            return m_Scene->m_Registry.get<T>(m_EntityHandle);
        }

        [[nodiscard]] auto GetUUID() const -> const UUID& { return GetComponent<IDComponent>().ID; }

        [[nodiscard]] auto GetName() const -> const std::string& { return GetComponent<TagComponent>().Tag; }

        operator EntityHandle() const { return m_EntityHandle; }
        operator bool() const { return m_EntityHandle != entt::null; }
        auto operator==(const Entity& other) const -> bool { return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene; }
        auto operator!=(const Entity& other) const -> bool { return !(*this == other); }

    private:
        EntityHandle m_EntityHandle = entt::null;
        Scene* m_Scene = nullptr;
    };
}