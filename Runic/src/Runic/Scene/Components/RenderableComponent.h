#pragma once

#include <glm.hpp>

#include "Runic/Graphics/Renderable.h"

namespace Runic
{
	struct RenderableComponent
	{
		RenderableComponent() = default;
		RenderableComponent(const RenderableComponent&) = default;

		Renderable renderable;
	};
}