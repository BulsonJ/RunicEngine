#pragma once

#include "Graphics/Renderer.h"
#include "RenderableTypes.h"

class Engine
{
public:
	void init();
	void run();
	void deinit();

private:
	void setupScene();

	Renderer rend;
	std::vector<RenderableTypes::RenderObject> renderObjects;
};

