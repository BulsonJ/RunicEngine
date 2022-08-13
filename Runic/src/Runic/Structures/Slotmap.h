#pragma once

#include <array>

/*
*
* Slotmap: Persistent handle to data. Not actual implementation, but at the moment simple array works best
*			until I need to be able to add new/delete buffers on a per frame basis.
*
*/

template<typename T>
class Slotmap
{
public:
	uint32_t add(const T& object);
	T& get(uint32_t handle);

	// TODO: make private
	std::array<T, 1024> m_array;
private:
	uint32_t lastHandle{ 0U };
	[[nodiscard]] uint32_t getNewHandle()
	{
		return ++lastHandle;
	}
};

template<typename T>
inline uint32_t Slotmap<T>::add(const T& object)
{
	uint32_t handle = getNewHandle();
	m_array[handle] = object;
	return handle;
}

template<typename T>
inline T& Slotmap<T>::get(uint32_t handle)
{
	return m_array[handle];
}
