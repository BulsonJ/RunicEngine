#pragma once

#include <optional>
#include "glm.hpp"

namespace Runic
{
	typedef uint32_t MeshHandle;
	typedef uint32_t TextureHandle;

	struct Renderable
	{
		MeshHandle meshHandle;

		std::optional<TextureHandle> textureHandle = {};
		std::optional<TextureHandle> normalHandle = {};
		std::optional<TextureHandle> roughnessHandle = {};
		std::optional<TextureHandle> emissionHandle = {};

		glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
	};

}

