#include "Runic/Scene/Components/TransformComponent.h"#

#define GLM_FORCE_RADIANS
#include <gtx/quaternion.hpp>

using namespace Runic;

glm::mat4 Runic::TransformComponent::BuildMatrix() const
{
	glm::mat4 modelMatrix = glm::translate(glm::mat4{ 1.0 }, translation)
		* glm::toMat4(glm::quat(rotation))
		* glm::scale(glm::mat4{ 1.0 }, scale);
	return modelMatrix;
}
