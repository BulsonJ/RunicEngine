#include "Runic/Engine.h"

#include <SDL.h>
#include <Tracy.hpp>
#include <backends/imgui_impl_sdl.h>

#include "Runic/Log.h"

using namespace Runic;

void Engine::init() {
	ZoneScoped;

	Log::Init();
	m_window.Init(WindowProps{ .title = "Runic Engine",.width = 1920U, .height = 1080U });
	m_rend.init(&m_window);
	setupScene();
}

void Engine::setupScene() {
	Runic::MeshDesc fileMesh;
	Runic::MeshHandle fileMeshHandle {};
	if (fileMesh.loadFromObj("../../assets/meshes/cube.obj"))
	{
		fileMeshHandle = m_rend.uploadMesh(fileMesh);
	}

	Runic::MeshDesc cubeMeshDesc = Runic::MeshDesc::GenerateCube();
	Runic::MeshHandle cubeMeshHandle = m_rend.uploadMesh(cubeMeshDesc);

	static const std::pair<std::string, Runic::TextureDesc::Format> texturePaths[] = {
		{"../../assets/textures/default.png", Runic::TextureDesc::Format::DEFAULT},
		{"../../assets/textures/texture.jpg", Runic::TextureDesc::Format::DEFAULT},
		{"../../assets/textures/metal/metal_albedo.png", Runic::TextureDesc::Format::DEFAULT},
		{"../../assets/textures/metal/metal_normal.png", Runic::TextureDesc::Format::NORMAL},
		{"../../assets/textures/bricks/bricks_albedo.png", Runic::TextureDesc::Format::DEFAULT},
		{"../../assets/textures/bricks/bricks_normal.png", Runic::TextureDesc::Format::NORMAL},
	};

	std::vector<Runic::TextureHandle> textures;
	for (int i = 0; i < std::size(texturePaths); ++i)
	{
		Runic::Texture img;
		const Runic::TextureDesc textureDesc{ .format = texturePaths[i].second };
		Runic::TextureUtil::LoadTextureFromFile(texturePaths[i].first.c_str(), textureDesc, img);
		Runic::TextureHandle texHandle = m_rend.uploadTexture(img);
		textures.push_back(texHandle);
	}

	for (int i = 0; i < 16; ++i)
	{
		for (int j = 0; j < 16; ++j)
		{
			const Runic::RenderObject materialTestObject{
				.meshHandle = cubeMeshHandle,
				.textureHandle = i > 1 ? textures[2] : textures[4],
				.normalHandle = i > 1 ? textures[3] : textures[5],
				.translation = { 1.0f * j,-0.5f,1.0f * i},
			};
			auto renderObj = m_scene.AddRenderObject();
			*renderObj = materialTestObject;
		}
	}

	m_scene.m_camera = std::make_unique<Camera>();

	LOG_CORE_INFO("Scene setup.");
}

void Engine::run()
{
	bool bQuit = { false };
	SDL_Event e;

	while (!bQuit)
	{
		while (SDL_PollEvent(&e) != 0)
		{
			ImGui_ImplSDL2_ProcessEvent(&e);
			if (e.type == SDL_QUIT)
			{
				bQuit = true;
			}
			if (e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_MINIMIZED)
			{
				m_rend.m_dirtySwapchain = true;
				m_window.Update();
				break;
			}
			const float speed = 0.1f;
			if (e.type == SDL_KEYDOWN)
			{
				if (e.key.keysym.sym == SDLK_w)
				{
					m_scene.m_camera.get()->m_pos = m_scene.m_camera.get()->m_pos + (speed * glm::vec3{ 0.0f, 0.0f, -1.0f });
				}
				if (e.key.keysym.sym == SDLK_s)
				{
					m_scene.m_camera.get()->m_pos = m_scene.m_camera.get()->m_pos + (speed * glm::vec3{ 0.0f, 0.0f, 1.0f });
				}
				if (e.key.keysym.sym == SDLK_a)
				{
					m_scene.m_camera.get()->m_pos = m_scene.m_camera.get()->m_pos + (speed * glm::vec3{ -1.0f, 0.0f, 0.0f });
				}
				if (e.key.keysym.sym == SDLK_d)
				{
					m_scene.m_camera.get()->m_pos = m_scene.m_camera.get()->m_pos + (speed * glm::vec3{ 1.0f, 0.0f, 0.0f });
				}
				if (e.key.keysym.sym == SDLK_q)
				{
					m_scene.m_camera.get()->m_pos = m_scene.m_camera.get()->m_pos + (speed * glm::vec3{ 0.0f, -1.0f, 0.0f });
				}
				if (e.key.keysym.sym == SDLK_e)
				{
					m_scene.m_camera.get()->m_pos = m_scene.m_camera.get()->m_pos + (speed * glm::vec3{ 0.0f, 1.0f, 0.0f });
				}
			}
		}
		m_rend.draw(m_scene.m_camera.get(), m_scene.m_renderObjects);
	}
}

void Engine::deinit()
{
	ZoneScoped;
	m_rend.deinit();
	m_window.Deinit();
}

