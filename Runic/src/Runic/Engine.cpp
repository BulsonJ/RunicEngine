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
	MeshDesc fileMesh;
	MeshHandle fileMeshHandle {};
	if (fileMesh.loadFromObj("../../assets/meshes/cube.obj"))
	{
		fileMeshHandle = m_rend.uploadMesh(fileMesh);
	}

	MeshDesc cubeMeshDesc = MeshDesc::GenerateCube();
	MeshHandle cubeMeshHandle = m_rend.uploadMesh(cubeMeshDesc);

	static const std::pair<std::string, TextureDesc::Format> texturePaths[] = {
		{"../../assets/textures/default.png", TextureDesc::Format::DEFAULT},
		{"../../assets/textures/texture.jpg", TextureDesc::Format::DEFAULT},
		{"../../assets/textures/metal/metal_albedo.png", TextureDesc::Format::DEFAULT},
		{"../../assets/textures/metal/metal_normal.png", TextureDesc::Format::NORMAL},
		{"../../assets/textures/bricks/bricks_albedo.png", TextureDesc::Format::DEFAULT},
		{"../../assets/textures/bricks/bricks_normal.png", TextureDesc::Format::NORMAL},
	};

	std::vector<TextureHandle> textures;
	for (int i = 0; i < std::size(texturePaths); ++i)
	{
		Texture img;
		const TextureDesc textureDesc{ .format = texturePaths[i].second };
		TextureUtil::LoadTextureFromFile(texturePaths[i].first.c_str(), textureDesc, img);
		TextureHandle texHandle = m_rend.uploadTexture(img);
		textures.push_back(texHandle);
	}
	const char* skyboxImagePaths[6] = {
		"../../assets/textures/skybox/skyrender0004.png",
		"../../assets/textures/skybox/skyrender0001.png",
		"../../assets/textures/skybox/skyrender0003.png",
		"../../assets/textures/skybox/skyrender0006.png",
		"../../assets/textures/skybox/skyrender0002.png",
		"../../assets/textures/skybox/skyrender0005.png",
	};
	Texture skyboxImage;
	TextureUtil::LoadCubemapFromFile(skyboxImagePaths, { .type = TextureDesc::Type::TEXTURE_CUBEMAP }, skyboxImage);
	const TextureHandle skybox = m_rend.uploadTexture(skyboxImage);
	m_rend.setSkybox(skybox);

	for (int i = 0; i < 16; ++i)
	{
		for (int j = 0; j < 16; ++j)
		{
			const RenderObject materialTestObject{
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
				if (e.key.keysym.sym == SDLK_r)
				{
					m_scene.m_camera.get()->m_yaw = m_scene.m_camera.get()->m_yaw + (speed * -10.0f);
				}
				if (e.key.keysym.sym == SDLK_t)
				{
					m_scene.m_camera.get()->m_yaw = m_scene.m_camera.get()->m_yaw + (speed * 10.0f);
				}
			}
		}
		std::vector<RenderObject*> renderObjects;
		renderObjects.reserve(m_scene.m_renderObjects.size());
		std::transform(m_scene.m_renderObjects.cbegin(), m_scene.m_renderObjects.cend(), std::back_inserter(renderObjects),
			[](auto& ptr) { return ptr.get(); });
		m_rend.draw(m_scene.m_camera.get(), renderObjects);
	}
}

void Engine::deinit()
{
	ZoneScoped;
	m_rend.deinit();
	m_window.Deinit();
}

