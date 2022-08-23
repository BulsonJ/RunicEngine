#pragma once

#include "Runic/Scene/Scene.h"

#include <entt/entt.hpp>
#include <assert.h>

namespace Runic
{
	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity handle, Runic::Scene* scene);
		Entity(const Entity& other) = default;

		template<typename T, typename... Args>
		T& AddComponent()
		{
			assert(!HasComponent<T>());
			return m_scene->m_registry.emplace<T>(m_entityHandle, std::forward<Args>()...);
		}

		template<typename T>
		T& RemoveComponent()
		{
			assert(!Component<T>());
			return m_scene->m_registry.remove<T>(m_entityHandle);
		}

		template<typename T>
		T& GetComponent()
		{
			assert(HasComponent<T>());
			return m_scene->m_registry.get<T>(m_entityHandle);
		}

		template<typename T>
		bool HasComponent()
		{
			return m_scene->m_registry.any_of<T>(m_entityHandle);
		}

		operator bool() const { return m_entityHandle != entt::null; }
	private:
		entt::entity m_entityHandle {entt::null};
		Runic::Scene* m_scene = nullptr;
	};
}