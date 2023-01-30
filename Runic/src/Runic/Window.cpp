#include "Runic/Window.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_vulkan.h>

void Runic::Window::Init(const WindowProps& props)
{
	SDL_Init(SDL_INIT_VIDEO);

	const SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	m_window = SDL_CreateWindow(
		"Runic Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		static_cast<int>(props.width),
		static_cast<int>(props.height),
		window_flags
	);
}

void Runic::Window::Deinit()
{
	SDL_DestroyWindow(reinterpret_cast<SDL_Window*>(m_window));
}

void Runic::Window::Update()
{
	int w, h;
	SDL_GetWindowSize(reinterpret_cast<SDL_Window*>(m_window), &w, &h);
	m_windowData.props.width = static_cast<uint32_t>(w);
	m_windowData.props.height = static_cast<uint32_t>(h);
}
