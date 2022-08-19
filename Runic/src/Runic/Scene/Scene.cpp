#include "Runic/Scene/Scene.h"

using namespace Runic;

std::shared_ptr<Renderable> Scene::AddRenderObject()
{
	std::shared_ptr<Renderable> newObj = std::make_shared<Renderable>();
	m_renderObjects.push_back(newObj);
	return newObj;
}
