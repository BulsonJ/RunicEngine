#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Runic/Graphics/Renderable.h"

namespace Runic
{
	class Renderer;

	class ModelLoader
	{
	public:
		ModelLoader(Renderer* rend);
		std::optional<std::vector<Renderable>> LoadModelFromObj(const std::string& filename);
		std::optional<std::vector<Renderable>> LoadModelFromGLTF(const std::string& filename);
	private:
		Renderer* m_rend;
	};
}
