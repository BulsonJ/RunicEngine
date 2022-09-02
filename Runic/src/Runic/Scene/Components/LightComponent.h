#pragma once

#include "glm.hpp"

struct LightComponent
{
	enum class LightType
	{
		Directional,
		Point,
	} lightType;

	glm::vec3 ambient = { 0.7f, 0.7f, 0.7f };
	glm::vec3 diffuse = { 1.0f,1.0f,1.0f};
	glm::vec3 specular = { 1.0f,1.0f,1.0f };

	// Point && Spot Lights
	glm::vec3 position = { 0.0f,0.0f,0.0f };

	// Directional
	glm::vec3 direction = { 0.0f, 0.0f, -1.0f};
};

