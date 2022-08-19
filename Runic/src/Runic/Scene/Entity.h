#pragma once

#include "Runic/Scene/Scene.h"

#include <entt/entt.hpp>

namespace Runic
{
	class Entity
	{
	public:
		Entity(entt::entity handle, Runic::Scene* scene);
		Entity(const Entity& other) = default;

		template<typename T>
		bool HasComponent()
		{
			return m_scene->m_registry.any_of<T>(m_entityHandle);
		}
	private:
		entt::entity m_entityHandle;
		Runic::Scene* m_scene;
	};
}