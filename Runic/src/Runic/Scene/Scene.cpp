#include "Runic/Scene/Scene.h"

using namespace Runic;

std::shared_ptr<RenderObject> Scene::AddRenderObject()
{
	std::shared_ptr<RenderObject> newObj = std::make_shared<RenderObject>();
	m_renderObjects.push_back(newObj);
	return newObj;
}
