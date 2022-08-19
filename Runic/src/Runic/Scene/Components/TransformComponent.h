#pragma once

#include <glm.hpp>

namespace Runic
{
	struct TransformComponent
	{
		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::mat4& transform) : transform(transform){}

		glm::mat4 transform;
	};
}