#pragma once

#include "Runic/Core.h"
#include "Graphics/Renderer.h"
#include "RenderableTypes.h"

#include <functional>

class RUNIC_API Engine
{
public:
	void init();
	void run(std::function<void()> main);
	void deinit();

private:
	void setupScene();

	Renderer rend;
	std::vector<RenderableTypes::RenderObject> renderObjects;
};

