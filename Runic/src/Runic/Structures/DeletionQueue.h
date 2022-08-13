#pragma once

#include <functional>

class DeletionQueue
{
public:
	/*
	Use to push a delete function onto the queue
	*/
	void push_function(std::function<void()>&& function);

	/*
	Calls all functions in queue,deleting all objects
	*/
	void flush();
private:
	std::vector<std::function<void()>> m_deletors;
};

