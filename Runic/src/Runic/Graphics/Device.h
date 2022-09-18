#pragma once
#include "Runic/Window.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

// internal
#include "Runic/Structures/DeletionQueue.h"

constexpr unsigned int FRAME_OVERLAP = 2U;

namespace Runic
{
	struct CommandContext
	{
		VkCommandPool pool;
		VkCommandBuffer buffer;
	};

	template<uint32_t FRAMES>
	struct QueueContext
	{
		VkQueue queue;
		uint32_t queueFamily;
		CommandContext commands[FRAMES];
	};

	struct UploadContext
	{
		VkFence uploadFence;
		VkCommandPool commandPool;
		VkCommandBuffer commandBuffer;
	};

	class Device
	{
	public:
		void init(Window* window);
		void deinit();

		// Move to private once device functions setup
		Window* m_window;

		VkInstance m_instance;
		VkPhysicalDevice m_chosenGPU;
		VkPhysicalDeviceProperties m_gpuProperties;
		VkDevice m_device;
		DeletionQueue m_instanceDeletionQueue;

		VkSurfaceKHR m_surface;
		VmaAllocator m_allocator;
		VkDebugUtilsMessengerEXT m_debugMessenger;

		Runic::QueueContext<FRAME_OVERLAP> m_graphics;
		Runic::QueueContext<1> m_compute;
		Runic::UploadContext m_uploadContext;
	private:
		void initVulkan();
	};
}
