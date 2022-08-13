#pragma once

#include "Runic/Core.h"
#include "Graphics/Renderer.h"

#include "Runic/Scene/Scene.h"

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

		Renderer m_rend;
		Scene m_scene;
	};
}

