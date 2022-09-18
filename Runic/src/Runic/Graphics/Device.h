#pragma once
#include "Runic/Window.h"

namespace Runic
{
	class Device
	{
	public:
		void init(Window* window);
		void deinit();

		// Move to private once device functions setup
		Window* m_window;
	private:
		
	};
}
