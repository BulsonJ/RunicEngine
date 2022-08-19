#pragma once

#include "Runic/Scene/Entity.h"

Runic::Entity::Entity(entt::entity handle, Scene* scene) : m_entityHandle(handle), m_scene(scene)
{

}