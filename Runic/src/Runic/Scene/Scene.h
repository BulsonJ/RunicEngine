#pragma once

#include <memory>
#include <vector>

#include <entt/entt.hpp>
#include <entt/entity/registry.hpp>

#include "Runic/Graphics/Renderable.h"
#include "Runic/Scene/Camera.h"


namespace Runic
{
	class Entity;

	class Scene
	{
		friend class Engine;
	public:
		Scene();
		~Scene();

		std::shared_ptr<Renderable> AddRenderObject();

		std::unique_ptr<Camera> m_camera;

		Entity CreateEntity();
	private:
		std::vector<std::shared_ptr<Renderable>> m_renderObjects;
		entt::registry m_registry;
	};
}