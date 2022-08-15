#pragma once

#include <string>

namespace Runic
{
	struct WindowProps
	{
		std::string title = {"Runic Engine"};
		uint32_t width = {1920U};
		uint32_t height = { 1080U };
	};

	class Window
	{
	public:
		void Init(const WindowProps& props);
		void Deinit();

		void Update();

		inline uint32_t GetWidth() const { return m_windowData.props.width; }
		inline uint32_t GetHeight() const { return m_windowData.props.height; }
		inline void* GetWindowPointer() const { return m_window; };
	private:
		struct WindowData
		{
			WindowProps props;
			bool vSync = {false};
		} m_windowData;
		void* m_window = {nullptr};
	};
}

