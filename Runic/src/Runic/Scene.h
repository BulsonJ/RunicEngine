#pragma once

#include <memory>
#include <vector>

#include "Runic/RenderObject.h"

namespace Runic
{
	class Scene
	{
		friend class Engine;
	public:
		std::shared_ptr<RenderObject> AddRenderObject();
	private:
		std::vector<std::shared_ptr<RenderObject>> renderObjects;
	};
}