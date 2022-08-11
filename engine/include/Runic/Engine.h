#pragma once

#include "Runic/Core.h"
#include "Graphics/Renderer.h"
#include "RenderableTypes.h"

class RUNIC_API Engine
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

