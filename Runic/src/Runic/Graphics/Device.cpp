#include "Runic/Graphics/Device.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>

#include <Tracy.hpp>
#include <common/TracySystem.hpp>

#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_vulkan.h>

#include "Runic/Editor.h"
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

void Device::Init(Window* window)
{
	m_window = window;

	initVulkan();
	createSwapchain();
	initGraphicsCommands();
	initComputeCommands();

	initSyncStructures();

	VkSamplerCreateInfo samplerInfo = VulkanInit::samplerCreateInfo(VK_FILTER_NEAREST);
	vkCreateSampler(m_device, &samplerInfo, nullptr, &m_defaultSampler);
	m_instanceDeletionQueue.push_function([=] {
		vkDestroySampler(m_device, m_defaultSampler, nullptr);
		});

	initImguiRenderpass();
	initImgui();
	initImguiRenderImages();

	m_pipelineManager = std::make_unique<PipelineManager>(m_device);
}

void Device::Deinit()
{
	m_pipelineManager->Deinit();
	delete ResourceManager::ptr;

	vkDeviceWaitIdle(m_device);

	m_instanceDeletionQueue.flush();

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

bool Device::BeginFrame()
{
	VK_CHECK(vkWaitForFences(m_device, 1, &GetCurrentFrame().renderFen, true, 1000000000));

	VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain.swapchain, 1000000000, GetCurrentFrame().presentSem, nullptr, &m_currentSwapchainImage);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_dirtySwapchain)
	{
		m_dirtySwapchain = false;
		recreateSwapchain();
		return false;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	VK_CHECK(vkResetFences(m_device, 1, &GetCurrentFrame().renderFen));
	VK_CHECK(vkResetCommandBuffer(m_graphics.commands[GetCurrentFrameNumber()].buffer, 0));

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
	Editor::DrawEditor();
	ImGui::Render();
	return true;
}

void Device::AddImGuiToCommandBuffer()
{
	const VkCommandBuffer cmd = m_graphics.commands[GetCurrentFrameNumber()].buffer;

	const VkImageMemoryBarrier2 imgMemBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR,
		.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.image = ResourceManager::ptr->GetImage(m_renderImage).image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	const VkImageMemoryBarrier2 depthShaderImgMemBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR,
		.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.image = ResourceManager::ptr->GetImage(m_depthImage).image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	VkImageMemoryBarrier2 shaderReadBarriers[] = { imgMemBarrier,depthShaderImgMemBarrier };

	const VkDependencyInfo dependencyInfo = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = std::size(shaderReadBarriers),
		.pImageMemoryBarriers = shaderReadBarriers,
	};

	vkCmdPipelineBarrier2(cmd, &dependencyInfo);

	const VkClearValue clearValue{
		.color = { 0.1f, 0.1f, 0.1f, 1.0f }
	};
	const VkExtent2D windowExtent{
		.width = m_window->GetWidth(),
		.height = m_window->GetHeight(),
	};
	VkRenderPassBeginInfo rpInfo = VulkanInit::renderpassBeginInfo(m_imguiPass, windowExtent, m_swapchain.framebuffers[m_currentSwapchainImage]);
	const VkClearValue clearValues[] = { clearValue };
	rpInfo.clearValueCount = 1;
	rpInfo.pClearValues = &clearValues[0];

	vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	vkCmdEndRenderPass(cmd);
}

void Device::EndFrame()
{

}

void Device::Present()
{
	const VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &GetCurrentFrame().renderSem,
		.swapchainCount = 1,
		.pSwapchains = &m_swapchain.swapchain,
		.pImageIndices = &m_currentSwapchainImage,
	};

	vkQueuePresentKHR(m_graphics.queue, &presentInfo);

	FrameMark;
	m_frameNumber++;
}

void Device::WaitIdle()
{
	vkDeviceWaitIdle(m_device);
}

void Device::WaitRender()
{
	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		vkWaitForFences(m_device, 1, &m_frame[i].renderFen, true, 1000000000);
	}
}

void Device::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	VkCommandBuffer cmd = m_uploadContext.commandBuffer;

	VkCommandBufferBeginInfo cmdBeginInfo = VulkanInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	vkBeginCommandBuffer(cmd, &cmdBeginInfo);

	function(cmd);

	vkEndCommandBuffer(cmd);

	VkSubmitInfo submit = VulkanInit::submitInfo(&cmd);

	vkQueueSubmit(m_graphics.queue, 1, &submit, m_uploadContext.uploadFence);

	vkWaitForFences(m_device, 1, &m_uploadContext.uploadFence, true, 9999999999);
	vkResetFences(m_device, 1, &m_uploadContext.uploadFence);

	vkResetCommandPool(m_device, m_uploadContext.commandPool, 0);
}

void Device::initVulkan()
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
	vkDestroyRenderPass(m_device, m_imguiPass, nullptr);
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
	initImguiRenderpass();
	initImguiRenderImages();
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
			.flags = 0,
		};

		vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_frame[i].presentSem);
		vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_frame[i].renderSem);
	}

	const VkFenceCreateInfo uploadFenceCreateInfo = VulkanInit::fenceCreateInfo();
	vkCreateFence(m_device, &uploadFenceCreateInfo, nullptr, &m_uploadContext.uploadFence);
}

void Device::initImguiRenderpass()
{
	const VkAttachmentDescription color_attachment = {
		.format = m_swapchain.imageFormat,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};

	const VkAttachmentReference color_attachment_ref = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
	};

	const VkSubpassDescription subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_ref,
	};

	VkRenderPassCreateInfo renderPassInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_attachment,
		.subpassCount = 1,
		.pSubpasses = &subpass,
	};

	const VkSubpassDependency dependency = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
	};

	const VkSubpassDependency dependencies[2] = { dependency };

	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependencies[0];

	VK_CHECK(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_imguiPass));

	VkFramebufferCreateInfo fb_info = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext = nullptr,
		.renderPass = m_imguiPass,
		.attachmentCount = 1,
		.width = m_window->GetWidth(),
		.height = m_window->GetHeight(),
		.layers = 1,
	};

	const int m_swapchain_imagecount = static_cast<int>(m_swapchain.images.size());
	m_swapchain.framebuffers = std::vector<VkFramebuffer>(m_swapchain_imagecount);

	//create m_framebuffers for each of the m_swapchain image views
	for (int i = 0; i < m_swapchain_imagecount; i++)
	{
		VkImageView attachment = m_swapchain.imageViews[i];

		fb_info.pAttachments = &attachment;
		fb_info.attachmentCount = 1;

		VK_CHECK(vkCreateFramebuffer(m_device, &fb_info, nullptr, &m_swapchain.framebuffers[i]));
	}
}

void Device::initImguiRenderImages()
{
	m_imguiRenderTexture = ImGui_ImplVulkan_AddTexture(m_defaultSampler, ResourceManager::ptr->GetImage(m_renderImage).imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_imguiDepthTexture = ImGui_ImplVulkan_AddTexture(m_defaultSampler, ResourceManager::ptr->GetImage(m_depthImage).imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	Editor::ViewportTexture = m_imguiRenderTexture;
	Editor::ViewportDepthTexture = m_imguiDepthTexture;
}

void Device::initImgui()
{
	// TODO : Fix when IMGUI adds dynamic rendering support
	// the size of the pool is very oversize, but it's copied from imgui demo itself.
	const VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	const VkDescriptorPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 1000,
		.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes)),
		.pPoolSizes = pool_sizes,
	};

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(m_device, &pool_info, nullptr, &imguiPool));

	ImGui::CreateContext();

	// light style from Pacôme Danhiez (user itamago) https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
	ImGuiStyle& style = ImGui::GetStyle();

	style.Alpha = 1.0f;
	style.FrameRounding = 3.0f;
	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
	//style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
	colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
	colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.98f, 0.59f, 0.26f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.98f, 0.59f, 0.26f, 0.67f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
	//style.Colors[ImGuiCol_ComboBg] = ImVec4(0.86f, 0.86f, 0.86f, 0.99f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.98f, 0.59f, 0.26f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.98f, 0.59f, 0.26f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.98f, 0.59f, 0.26f, 0.40f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.98f, 0.59f, 0.26f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.98f, 0.59f, 0.26f, 0.31f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.98f, 0.59f, 0.26f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.98f, 0.59f, 0.26f, 1.00f);
	//style.Colors[ImGuiCol_Column] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	//style.Colors[ImGuiCol_ColumnHovered] = ImVec4(0.98f, 0.59f, 0.26f, 0.78f);
	//style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.98f, 0.59f, 0.26f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.98f, 0.59f, 0.26f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.98f, 0.59f, 0.26f, 0.95f);
	//style.Colors[ImGuiCol_CloseButton] = ImVec4(0.59f, 0.59f, 0.59f, 0.50f);
	//style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);
	//style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.98f, 0.59f, 0.26f, 0.35f);
	//style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

	for (int i = 0; i <= ImGuiCol_COUNT; i++)
	{
		ImVec4& col = style.Colors[i];
		float H, S, V;
		ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, H, S, V);

		if (S < 0.1f)
		{
			V = 1.0f - V;
		}
		ImGui::ColorConvertHSVtoRGB(H, S, V, col.x, col.y, col.z);
		if (col.w < 1.00f)
		{
			col.w *= 1.0f;
		}
	}

	ImGui_ImplSDL2_InitForVulkan(reinterpret_cast<SDL_Window*>(m_window->GetWindowPointer()));

	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {
		.Instance = m_instance,
		.PhysicalDevice = m_chosenGPU,
		.Device = m_device,
		.Queue = m_graphics.queue,
		.DescriptorPool = imguiPool,
		.MinImageCount = 3,
		.ImageCount = 3,
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
	};

	ImGui_ImplVulkan_Init(&init_info, m_imguiPass);

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("../../assets/fonts/Roboto-Medium.ttf", 13);
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	//execute a gpu command to upload imgui font textures
	ImmediateSubmit([&](VkCommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
		});
	//
	////clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	m_instanceDeletionQueue.push_function([=] {
		vkDestroyDescriptorPool(m_device, imguiPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		});
}

