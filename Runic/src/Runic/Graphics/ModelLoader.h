#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Runic/Scene/Components/RenderableComponent.h"

namespace Runic
{
	class Renderer;

	class ModelLoader
	{
	public:
		ModelLoader(Renderer* rend);
		std::optional<std::vector<RenderableComponent>> LoadModelFromObj(const std::string& filename);
		std::optional<std::vector<RenderableComponent>> LoadModelFromGLTF(const std::string& filename);
	private:
		Renderer* m_rend;
	};
}
