#pragma once

#include <glm.hpp>

namespace Runic
{
	struct TransformComponent
	{
		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;

		glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
	};
}