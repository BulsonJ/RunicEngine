#pragma once

#include <glm.hpp>
#include <optional>

namespace Runic
{
	typedef uint32_t MeshHandle;
	typedef uint32_t TextureHandle;

	struct RenderableComponent
	{
		RenderableComponent() = default;
		RenderableComponent(const RenderableComponent&) = default;

		MeshHandle meshHandle;

		std::optional<TextureHandle> textureHandle = {};
		std::optional<TextureHandle> normalHandle = {};
		std::optional<TextureHandle> roughnessHandle = {};
		std::optional<TextureHandle> emissionHandle = {};
	};
}