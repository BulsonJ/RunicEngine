#pragma once

#include "Runic/Core.h"
#include "Graphics/Renderer.h"
#include "Runic/RenderObject.h"

namespace Runic
{
	class Engine
	{
	public:
		void init();
		void run();
		void deinit();

	private:
		void setupScene();

		Renderer rend;
		std::vector<RenderObject> renderObjects;
	};
}

