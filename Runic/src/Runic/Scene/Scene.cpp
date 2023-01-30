#include "Runic/Scene/Scene.h"
#include "Runic/Scene/Entity.h"

using namespace Runic;

Scene::Scene()
{

}

Scene::~Scene() 
{

}

std::shared_ptr<Entity> Scene::CreateEntity()
{
	std::shared_ptr<Entity> ent = std::make_shared<Entity>(m_registry.create(), this);
	m_entities.push_back(ent);
	return ent;
}


