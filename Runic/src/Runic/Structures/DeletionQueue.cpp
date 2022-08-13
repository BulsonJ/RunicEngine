#include "Runic/DeletionQueue.h"

void DeletionQueue::push_function(std::function<void()>&& function)
{
	m_deletors.push_back(function);
}

void DeletionQueue::flush()
{
	for (auto it = m_deletors.rbegin(); it != m_deletors.rend(); it++)
	{
		(*it)();
	}

	m_deletors.clear();
}
