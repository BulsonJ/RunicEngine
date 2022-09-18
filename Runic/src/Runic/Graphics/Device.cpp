#include "Runic/Graphics/Device.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>

#include <Tracy.hpp>
#include <common/TracySystem.hpp>

#include "Runic/Graphics/ResourceManager.h"
#include "Runic/Graphics/Internal/VulkanInit.h"
#include "Runic/Log.h"

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			LOG_CORE_ERROR("Detected Vulkan error: " + err); \
			abort();                                                \
		}                                                           \
	} while (0)

using namespace Runic;

void Device::init(Window* window)
{
	m_window = window;

	initVulkan();
	initGraphicsCommands();
	initComputeCommands();
	createSwapchain();
	initSyncStructures();
}

void Device::deinit()
{
	delete ResourceManager::ptr;

	vkDeviceWaitIdle(m_device);

	vkDestroyFence(m_device, m_uploadContext.uploadFence, nullptr);

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		vkDestroySemaphore(m_device, m_frame[i].presentSem, nullptr);
		vkDestroySemaphore(m_device, m_frame[i].renderSem, nullptr);
		vkDestroyFence(m_device, m_frame[i].renderFen, nullptr);
	}

	vkDestroyCommandPool(m_device, m_uploadContext.commandPool, nullptr);

	vkDestroyCommandPool(m_device, m_graphics.commands[0].pool, nullptr);
	vkDestroyCommandPool(m_device, m_graphics.commands[1].pool, nullptr);
	vkDestroyCommandPool(m_device, m_compute.commands[0].pool, nullptr);

	vmaDestroyAllocator(m_allocator);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
	vkDestroyDevice(m_device, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}

void Runic::Device::BeginFrame()
{
	VK_CHECK(vkWaitForFences(m_device, 1, &getCurrentFrame().renderFen, true, 1000000000));

	VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain.swapchain, 1000000000, getCurrentFrame().presentSem, nullptr, &m_currentSwapchainImage);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_dirtySwapchain)
	{
		m_dirtySwapchain = false;
		recreateSwapchain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	VK_CHECK(vkResetFences(m_device, 1, &getCurrentFrame().renderFen));
	VK_CHECK(vkResetCommandBuffer(m_graphics.commands[getCurrentFrameNumber()].buffer, 0));
}

void Runic::Device::EndFrame()
{
	m_frameNumber++;
}

void Runic::Device::Present()
{
	const VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &getCurrentFrame().renderSem,
		.swapchainCount = 1,
		.pSwapchains = &m_swapchain.swapchain,
		.pImageIndices = &m_currentSwapchainImage,
		};

	vkQueuePresentKHR(m_graphics.queue, &presentInfo);
}

void Runic::Device::WaitIdle()
{
	vkDeviceWaitIdle(m_device);
}

void Runic::Device::WaitRender()
{
	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		vkWaitForFences(m_device, 1, &m_frame[i].renderFen, true, 1000000000);
	}
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


void Device::initGraphicsCommands()
{
	ZoneScoped;

	const VkCommandPoolCreateInfo m_graphicsCommandPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_graphics.queueFamily
	};

	for (int i = 0; i < 2; ++i)
	{
		VkCommandPool* commandPool = &m_graphics.commands[i].pool;

		vkCreateCommandPool(m_device, &m_graphicsCommandPoolCreateInfo, nullptr, commandPool);

		const VkCommandBufferAllocateInfo bufferAllocInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = *commandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		vkAllocateCommandBuffers(m_device, &bufferAllocInfo, &m_graphics.commands[i].buffer);
	}


	const VkCommandPoolCreateInfo uploadCommandPoolInfo = VulkanInit::commandPoolCreateInfo(m_graphics.queueFamily);
	vkCreateCommandPool(m_device, &uploadCommandPoolInfo, nullptr, &m_uploadContext.commandPool);

	const VkCommandBufferAllocateInfo cmdAllocInfo = VulkanInit::commandBufferAllocateInfo(m_uploadContext.commandPool, 1);
	vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &m_uploadContext.commandBuffer);
}

void Device::initComputeCommands()
{
	ZoneScoped;
	const VkCommandPoolCreateInfo computeCommandPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_compute.queueFamily
	};

	VkCommandPool* commandPool = &m_compute.commands[0].pool;

	vkCreateCommandPool(m_device, &computeCommandPoolCreateInfo, nullptr, commandPool);

	const VkCommandBufferAllocateInfo bufferAllocInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = *commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	vkAllocateCommandBuffers(m_device, &bufferAllocInfo, &m_compute.commands[0].buffer);
}

void Device::createSwapchain()
{
	ZoneScoped;
	vkb::SwapchainBuilder m_swapchainBuilder{ m_chosenGPU,m_device,m_surface };

	const VkExtent2D windowExtent{
		.width = m_window->GetWidth(),
		.height = m_window->GetHeight(),
	};

	vkb::Swapchain vkbm_swapchain = m_swapchainBuilder
		.use_default_format_selection()
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(windowExtent.width, windowExtent.height)
		.build()
		.value();

	m_swapchain.swapchain = vkbm_swapchain.swapchain;
	m_swapchain.images = vkbm_swapchain.get_images().value();
	m_swapchain.imageViews = vkbm_swapchain.get_image_views().value();
	m_swapchain.imageFormat = vkbm_swapchain.image_format;

	const VkDeviceSize imageSize = { static_cast<VkDeviceSize>(windowExtent.height * windowExtent.width * 4) };
	const VkFormat image_format{ VK_FORMAT_R8G8B8A8_SRGB };

	const VkExtent3D imageExtent{
		.width = static_cast<uint32_t>(windowExtent.width),
		.height = static_cast<uint32_t>(windowExtent.height),
		.depth = 1,
	};

	const VkImageCreateInfo imageInfo = VulkanInit::imageCreateInfo(image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, imageExtent);
	const VkImageCreateInfo m_depthImageInfo = VulkanInit::imageCreateInfo(DEPTH_FORMAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, imageExtent);

	m_renderImage = ResourceManager::ptr->CreateImage(ImageCreateInfo{
			.imageInfo = imageInfo,
			.imageType = ImageCreateInfo::ImageType::TEXTURE_2D,
			.usage = ImageCreateInfo::Usage::COLOR
		});


	m_depthImage = ResourceManager::ptr->CreateImage(ImageCreateInfo{
			.imageInfo = m_depthImageInfo,
			.imageType = ImageCreateInfo::ImageType::TEXTURE_2D,
			.usage = ImageCreateInfo::Usage::DEPTH
		});
	LOG_CORE_INFO("Create m_swapchain");
}

void Device::destroySwapchain()
{
	for (int i = 0; i < m_swapchain.imageViews.size(); i++)
	{
		vkDestroyFramebuffer(m_device, m_swapchain.framebuffers[i], nullptr);
		vkDestroyImageView(m_device, m_swapchain.imageViews[i], nullptr);
	}
	//vkDestroyRenderPass(m_device, m_imguiPass, nullptr);
	vkDestroySwapchainKHR(m_device, m_swapchain.swapchain, nullptr);

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		ResourceManager::ptr->DestroyImage(m_renderImage);
	};
	LOG_CORE_INFO("Destroy Swapchain");
}

void Device::recreateSwapchain()
{
	ZoneScoped;

	SDL_Event e;
	int flags = SDL_GetWindowFlags(reinterpret_cast<SDL_Window*>(m_window->GetWindowPointer()));
	bool minimized = (flags & SDL_WINDOW_MINIMIZED) ? true : false;
	while (minimized)
	{
		flags = SDL_GetWindowFlags(reinterpret_cast<SDL_Window*>(m_window->GetWindowPointer()));
		minimized = (flags & SDL_WINDOW_MINIMIZED) ? true : false;
		SDL_WaitEvent(&e);
	}

	vkDeviceWaitIdle(m_device);

	destroySwapchain();
	createSwapchain();
	//initImguiRenderpass();
	//initImguiRenderImages();
}

void Device::initSyncStructures()
{
	ZoneScoped;

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		const VkFenceCreateInfo fenceCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};

		vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_frame[i].renderFen);

		const VkSemaphoreCreateInfo semaphoreCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0
		};

		vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_frame[i].presentSem);
		vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_frame[i].renderSem);
	}


	const VkFenceCreateInfo uploadFenceCreateInfo = VulkanInit::fenceCreateInfo();
	vkCreateFence(m_device, &uploadFenceCreateInfo, nullptr, &m_uploadContext.uploadFence);


}

