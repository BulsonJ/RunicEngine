#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Runic/Graphics/RenderObject.h"

namespace Runic
{
	class Renderer;

	class ModelLoader
	{
	public:
		ModelLoader(Renderer* rend);
		std::optional<std::vector<RenderObject>> LoadModelFromObj(const std::string& filename);
		std::optional<std::vector<RenderObject>> LoadModelFromGLTF(const std::string& filename);
	private:
		Renderer* m_rend;
	};
}
