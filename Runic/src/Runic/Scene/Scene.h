#pragma once

#include <memory>
#include <vector>

#include <entt/entt.hpp>
#include <entt/entity/registry.hpp>

#include "Runic/Scene/Camera.h"


namespace Runic
{
	class Entity;

	class Scene
	{
		friend class Engine;
		friend class Entity;
	public:
		Scene();
		~Scene();

		std::unique_ptr<Camera> m_camera;
		std::vector<std::shared_ptr<Entity>> m_entities;
		std::shared_ptr<Entity> CreateEntity();
	private:

		entt::registry m_registry;
	};
}