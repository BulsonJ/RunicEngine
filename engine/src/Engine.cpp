#include "Engine.h"
#include <SDL.h>
#include <Tracy.hpp>
#include <backends/imgui_impl_sdl.h>

#include "Log.h"

void Engine::init() {
	ZoneScoped;

	Log::Init();
	rend.init();
	setupScene();
}

void Engine::setupScene() {
	RenderableTypes::MeshDesc fileMesh;
	RenderableTypes::MeshHandle fileMeshHandle {};
	if (fileMesh.loadFromObj("../assets/meshes/cube.obj"))
	{
		fileMeshHandle = rend.uploadMesh(fileMesh);
	}

	RenderableTypes::MeshDesc cubeMeshDesc = RenderableTypes::MeshDesc::GenerateCube();
	RenderableTypes::MeshHandle cubeMeshHandle = rend.uploadMesh(cubeMeshDesc);

	static const std::pair<std::string, RenderableTypes::TextureDesc::Format> texturePaths[] = {
		{"../assets/textures/default.png", RenderableTypes::TextureDesc::Format::DEFAULT},
		{"../assets/textures/texture.jpg", RenderableTypes::TextureDesc::Format::DEFAULT},
		{"../assets/textures/metal/metal_albedo.png", RenderableTypes::TextureDesc::Format::DEFAULT},
		{"../assets/textures/metal/metal_normal.png", RenderableTypes::TextureDesc::Format::NORMAL},
		{"../assets/textures/bricks/bricks_albedo.png", RenderableTypes::TextureDesc::Format::DEFAULT},
		{"../assets/textures/bricks/bricks_normal.png", RenderableTypes::TextureDesc::Format::NORMAL},
	};

	std::vector<RenderableTypes::TextureHandle> textures;
	for (int i = 0; i < std::size(texturePaths); ++i)
	{
		RenderableTypes::Texture img;
		const RenderableTypes::TextureDesc textureDesc{ .format = texturePaths[i].second };
		RenderableTypes::TextureUtil::LoadTextureFromFile(texturePaths[i].first.c_str(), textureDesc, img);
		RenderableTypes::TextureHandle texHandle = rend.uploadTexture(img);
		textures.push_back(texHandle);
	}

	for (int i = 0; i < 6; ++i)
	{
		for (int j = 0; j < 6; ++j)
		{
			const RenderableTypes::RenderObject materialTestObject{
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
				rend.window.resized = true;
				break;
			}
		}
		rend.draw(renderObjects);
	}
}

void Engine::deinit()
{
	ZoneScoped;
	rend.deinit();
}