#pragma once

#include "Runic/Core.h"

namespace Runic
{
	class RUNIC_API Application
	{
	public:
		Application() {};
		virtual ~Application(){};

		virtual void Run() {};
	};

	// To be defined in client
	Application* CreateApplication();

}