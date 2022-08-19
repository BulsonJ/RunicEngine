#include "Runic/Scene/Scene.h"
#include "Runic/Scene/Entity.h"

using namespace Runic;

Scene::Scene()
{

}

Scene::~Scene() 
{

}

Entity Scene::CreateEntity()
{
	return { m_registry.create(), this};
}

std::shared_ptr<Renderable> Scene::AddRenderObject()
{
	std::shared_ptr<Renderable> newObj = std::make_shared<Renderable>();
	m_renderObjects.push_back(newObj);
	return newObj;
}


