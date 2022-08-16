#pragma once

#include <optional>
#include <string>

#include "Runic/Graphics/RenderObject.h"

namespace Runic
{
	class Renderer;

	class ModelLoader
	{
	public:
		ModelLoader(Renderer* rend);
		std::optional<RenderObject> LoadModelFromObj(const std::string& filename);
	private:
		Renderer* m_rend;
	};
}
