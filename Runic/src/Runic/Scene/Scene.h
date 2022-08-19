#pragma once

#include <memory>
#include <vector>

#include "Runic/Graphics/Renderable.h"
#include "Runic/Scene/Camera.h"

namespace Runic
{
	class Scene
	{
		friend class Engine;
	public:
		std::shared_ptr<Renderable> AddRenderObject();

		std::unique_ptr<Camera> m_camera;
	private:
		std::vector<std::shared_ptr<Renderable>> m_renderObjects;
	};
}