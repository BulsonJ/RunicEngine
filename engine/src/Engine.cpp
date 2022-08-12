#include "Runic/Engine.h"

#include <SDL.h>
#include <Tracy.hpp>
#include <backends/imgui_impl_sdl.h>

#include "Runic/Log.h"

using namespace Runic;

void Engine::init() {
	ZoneScoped;

	Log::Init();
	rend.init();
	setupScene();
}

void Engine::setupScene() {
	Runic::MeshDesc fileMesh;
	Runic::MeshHandle fileMeshHandle {};
	if (fileMesh.loadFromObj("../assets/meshes/cube.obj"))
	{
		fileMeshHandle = rend.uploadMesh(fileMesh);
	}

	Runic::MeshDesc cubeMeshDesc = Runic::MeshDesc::GenerateCube();
	Runic::MeshHandle cubeMeshHandle = rend.uploadMesh(cubeMeshDesc);

	static const std::pair<std::string, Runic::TextureDesc::Format> texturePaths[] = {
		{"../assets/textures/default.png", Runic::TextureDesc::Format::DEFAULT},
		{"../assets/textures/texture.jpg", Runic::TextureDesc::Format::DEFAULT},
		{"../assets/textures/metal/metal_albedo.png", Runic::TextureDesc::Format::DEFAULT},
		{"../assets/textures/metal/metal_normal.png", Runic::TextureDesc::Format::NORMAL},
		{"../assets/textures/bricks/bricks_albedo.png", Runic::TextureDesc::Format::DEFAULT},
		{"../assets/textures/bricks/bricks_normal.png", Runic::TextureDesc::Format::NORMAL},
	};

	std::vector<Runic::TextureHandle> textures;
	for (int i = 0; i < std::size(texturePaths); ++i)
	{
		Runic::Texture img;
		const Runic::TextureDesc textureDesc{ .format = texturePaths[i].second };
		Runic::TextureUtil::LoadTextureFromFile(texturePaths[i].first.c_str(), textureDesc, img);
		Runic::TextureHandle texHandle = rend.uploadTexture(img);
		textures.push_back(texHandle);
	}

	for (int i = 0; i < 6; ++i)
	{
		for (int j = 0; j < 6; ++j)
		{
			const Runic::RenderObject materialTestObject{
				.meshHandle = cubeMeshHandle,
				.textureHandle = i > 3 ? textures[2] : textures[4],
				.normalHandle = i > 3 ? textures[3] : textures[5],
				.translation = { 1.0f * j,-0.5f,1.0f * i},
			};
			renderObjects.push_back(materialTestObject);
		}
	}


	LOG_CORE_INFO("Scene setup.");
}

void Engine::run(std::function<void()> applicationMain)
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
				rend.window.resized = true;
				break;
			}
		}
		rend.draw(renderObjects);
		applicationMain();
	}
}

void Engine::deinit()
{
	ZoneScoped;
	rend.deinit();
}