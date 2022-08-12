#include "Runic/Scene/Camera.h"
#define GLM_FORCE_RADIANS
#include <gtx/transform.hpp>

using namespace Runic;

const glm::mat4 Camera::BuildViewMatrix() const
{
	return glm::rotate(glm::radians(-pitch), glm::vec3{ 1.0f, 0.0f, 0.0f }) *
		glm::rotate(glm::radians(-yaw), glm::vec3{ 0.0f, 1.0f, 0.0f }) *
		glm::translate(-pos);
}

const glm::mat4 Camera::BuildProjMatrix() const
{
	glm::mat4 projTemp = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 1000.0f);
	projTemp[1][1] *= -1;
	return projTemp;
}