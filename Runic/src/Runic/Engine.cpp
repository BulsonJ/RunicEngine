#include "Runic/Engine.h"

#include <SDL.h>
#include <Tracy.hpp>
#include <backends/imgui_impl_sdl.h>

#include "Runic/Log.h"
#include "Runic/Graphics/ModelLoader.h"
#include "Runic/Scene/Components/TransformComponent.h"

using namespace Runic;

void Engine::init() {
	ZoneScoped;

	Log::Init();
	m_window.Init(WindowProps{ .title = "Runic Engine",.width = 1920U, .height = 1080U });
	m_device.init(&m_window);
	m_rend.init(&m_device);

	setupScene();
}

void Engine::setupScene() {
	ModelLoader loader(&m_rend);

	if (std::optional<std::vector<RenderableComponent>> sponzaObject = loader.LoadModelFromGLTF("../../assets/models/DamagedHelmet/DamagedHelmet.gltf"); sponzaObject.has_value())
	{
		for (const auto& rendObj : sponzaObject.value())
		{
			m_obj = m_scene.CreateEntity();
			RenderableComponent* sponzaValue = &m_obj->AddComponent<RenderableComponent>();
			*sponzaValue = rendObj;

			TransformComponent* transform = &m_obj->AddComponent<TransformComponent>();
			transform->translation = { 5.0f, 2.0f, 0.0f };
			transform->rotation = {1.25f, -0.5f,0.0f};
			transform->scale = { 2.0f,2.0f,2.0f };
		}
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
	skyboxImage.destroy();

	std::shared_ptr<Entity> sun = m_scene.CreateEntity();
	LightComponent* sunlight = &sun->AddComponent<LightComponent>();
	sunlight->lightType = LightComponent::LightType::Directional;
	sunlight->direction = { -0.15f, 0.1f, 0.4f };
	sunlight->diffuse = { 1.0f,1.0f,1.0f};
	sunlight->ambient = { 0.7f, 0.7f, 0.7f};

	for (int i = 0; i < 4; ++i)
	{
		std::shared_ptr<Entity> pointLightEntity = m_scene.CreateEntity();
		LightComponent* pointLight = &pointLightEntity->AddComponent<LightComponent>();
		pointLight->lightType = LightComponent::LightType::Point;
		pointLight->position = { 25.0f * i, 0.0f, 0.0f };
		pointLight->diffuse = { 0.0f,1.0f,1.0f };
		pointLight->ambient = { 0.7f, 0.7f, 0.7f };
	}

	m_scene.m_camera = std::make_unique<Camera>();
	m_scene.m_camera->m_yaw = -90.0f;
	m_scene.m_camera->m_pos = { 0.0f, 2.0f, 0.0f };

	m_rend.GiveRenderables(m_scene.m_entities);

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
		if (m_obj.get() && m_obj->HasComponent<TransformComponent>())
		{
			m_obj->GetComponent<TransformComponent>().rotation = m_obj->GetComponent<TransformComponent>().rotation + glm::vec3{0.0f, 0.001f, 0.0f};
		}
		m_rend.draw(m_scene.m_camera.get());
	}
}

void Engine::deinit()
{
	ZoneScoped;
	m_rend.deinit();
	m_window.Deinit();
}

