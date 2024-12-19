#include "pch.h"
#include "Entity.h"

namespace Eppo
{
	Entity::Entity(const EntityHandle entityHandle, Scene* scene)
		: m_EntityHandle(entityHandle), m_Scene(scene)
	{}
}
