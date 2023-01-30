#pragma once

#include "Runic/Core.h"
#include "Graphics/Renderer.h"

#include "Runic/Window.h"
#include "Runic/Scene/Scene.h"
#include "Runic/Graphics/Device.h"

namespace Runic
{
	class Engine
	{
	public:
		void Init();
		void run();
		void Deinit();
	private:
		void setupScene();

		Window m_window;
		Device m_device;
		Renderer m_rend;
		Scene m_scene;

		std::shared_ptr<Entity> m_obj;
	};
}

