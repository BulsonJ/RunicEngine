#include "Runic/Graphics/Device.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>

#include <Tracy.hpp>
#include <common/TracySystem.hpp>

#include "Runic/Graphics/ResourceManager.h"
#include "Runic/Log.h"

using namespace Runic;

void Device::init(Window* window)
{
	m_window = window;

	initVulkan();
}

void Device::deinit()
{
	vmaDestroyAllocator(m_allocator);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
	vkDestroyDevice(m_device, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}

void Runic::Device::initVulkan()
{
	ZoneScoped;
	vkb::InstanceBuilder builder;

	const auto inst_ret = builder.set_app_name("Runic Engine")
		.request_validation_layers(true)
		.require_api_version(1, 3, 0)
		.enable_extension("VK_EXT_debug_utils")
		.use_default_debug_messenger()
		.build();

	const vkb::Instance vkb_inst = inst_ret.value();

	m_instance = vkb_inst.instance;
	m_debugMessenger = vkb_inst.debug_messenger;

	SDL_Vulkan_CreateSurface(reinterpret_cast<SDL_Window*>(m_window->GetWindowPointer()), m_instance, &m_surface);

	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	const vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 2)
		.set_surface(m_surface)
		.select()
		.value();

	vkb::DeviceBuilder m_deviceBuilder{ physicalDevice };

	VkPhysicalDeviceSynchronization2Features syncFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
		.synchronization2 = true,
	};

	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
		.pNext = &syncFeatures,
		.dynamicRendering = VK_TRUE,
	};

	VkPhysicalDeviceDescriptorIndexingFeatures descIndexFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
		.pNext = &dynamicRenderingFeature,
		.shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
		.descriptorBindingPartiallyBound = VK_TRUE,
		.descriptorBindingVariableDescriptorCount = VK_TRUE,
		.runtimeDescriptorArray = VK_TRUE,
	};

	const vkb::Device vkbDevice = m_deviceBuilder
		.add_pNext(&descIndexFeatures)
		.build()
		.value();

	m_device = vkbDevice.device;
	m_chosenGPU = physicalDevice.physical_device;
	m_gpuProperties = vkbDevice.physical_device.properties;

	m_graphics.queue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	m_graphics.queueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
	m_compute.queue = vkbDevice.get_queue(vkb::QueueType::compute).value();
	m_compute.queueFamily = vkbDevice.get_queue_index(vkb::QueueType::compute).value();

	const VmaAllocatorCreateInfo m_allocatorInfo = {
		.physicalDevice = m_chosenGPU,
		.device = m_device,
		.instance = m_instance,
	};
	vmaCreateAllocator(&m_allocatorInfo, &m_allocator);

	ResourceManager::ptr = new ResourceManager(m_device, m_allocator);
	LOG_CORE_INFO("Vulkan Initialised");
}


