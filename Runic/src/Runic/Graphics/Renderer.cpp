#include "Runic/Graphics/Renderer.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <VkBootstrap.h>

#include <Tracy.hpp>
#include <common/TracySystem.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/transform.hpp>
#include <gtx/quaternion.hpp>

#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_vulkan.h>

#include <iostream>
#include <memory>
#include <unordered_set>
#include <stb_image.h>

#include "Runic/Graphics/Internal/VulkanInit.h"
#include "Runic/Editor.h"
#include "Runic/Log.h"

using namespace Runic;

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


void Renderer::init(Window* window)
{
	ZoneScoped;

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


	initShaders();

	initShaderData();



}

void Renderer::initShaderData()
{
	ZoneScoped;

	m_skybox.meshHandle = uploadMesh(MeshDesc::GenerateSkyboxCube());
	m_skybox.textureHandle = 0;

	Editor::lightDirection = &m_sunlight.direction;
	Editor::lightColor = &m_sunlight.color;
	Editor::lightAmbientColor = &m_sunlight.ambientColor;
}

void Renderer::drawObjects(VkCommandBuffer cmd, const std::vector<RenderObject*>& renderObjects)
{
	ZoneScoped;
	const int COUNT = static_cast<int>(renderObjects.size());
	const Runic::RenderObject* FIRST = renderObjects[0];

	// fill buffers
	// binding 0
		//slot 0 - transform
	GPUData::DrawData* drawDataSSBO = (GPUData::DrawData*)ResourceManager::ptr->GetBuffer(getCurrentFrame().drawDataBuffer).ptr;
	GPUData::Transform* objectSSBO = (GPUData::Transform*)ResourceManager::ptr->GetBuffer(getCurrentFrame().transformBuffer).ptr;
	GPUData::Material* materialSSBO = (GPUData::Material*)ResourceManager::ptr->GetBuffer(getCurrentFrame().materialBuffer).ptr;

	for (int i = 0; i < COUNT; ++i)
	{
		const Runic::RenderObject& object = *renderObjects[i];

		drawDataSSBO[i].transformIndex = i;
		drawDataSSBO[i].materialIndex = i;

		const glm::mat4 modelMatrix = glm::translate(glm::mat4{ 1.0 }, object.translation)
			* glm::toMat4(glm::quat(object.rotation))
			* glm::scale(glm::mat4{ 1.0 }, object.scale);
		objectSSBO[i].modelMatrix = modelMatrix;
		objectSSBO[i].normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

		materialSSBO[i] = GPUData::Material{
			.specular = {0.4f,0.4,0.4f},
			.shininess = 64.0f,
			.textureIndices = {object.textureHandle.has_value() ? object.textureHandle.value() : -1,
							object.normalHandle.has_value() ? object.normalHandle.value() : -1,
							0,
							0},
		};
	}

	int skyboxCount = COUNT;
	drawDataSSBO[skyboxCount].transformIndex = skyboxCount;
	drawDataSSBO[skyboxCount].materialIndex = skyboxCount;

	materialSSBO[skyboxCount] = GPUData::Material{
			.textureIndices = {m_skybox.textureHandle.value(),
							0,
							0,
							0},
	};


	// binding 1
		//slot 0 - camera
	//const float rotationSpeed = 0.5f;
	//camera.view = glm::rotate(camera.view, (m_frameNumber / 120.0f) * rotationSpeed, UP_DIR);
	GPUData::Camera* cameraSSBO = (GPUData::Camera*)ResourceManager::ptr->GetBuffer(getCurrentFrame().cameraBuffer).ptr;
	cameraSSBO->view = m_currentCamera->BuildViewMatrix();
	cameraSSBO->proj = m_currentCamera->BuildProjMatrix();
	cameraSSBO->pos = {m_currentCamera->GetPosition(), 0.0f};
		//slot 1 - directionalLight
	GPUData::DirectionalLight* dirLightSSBO = (GPUData::DirectionalLight*)ResourceManager::ptr->GetBuffer(getCurrentFrame().dirLightBuffer).ptr;
	*dirLightSSBO = m_sunlight;

	const MaterialType* lastMaterialType = nullptr;
	const RenderMesh* lastMesh = nullptr;
	for (int i = 0; i < COUNT + 1; ++i)
	{
		const Runic::RenderObject& object = i != COUNT ? *renderObjects[i] : m_skybox;

		// TODO : RenderObjects hold material handle for different m_materials
		const MaterialType* currentMaterialType{ i != COUNT ? &m_materials["defaultMaterial"] : &m_materials["skyboxMaterial"]};
		if (currentMaterialType != lastMaterialType)
		{
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentMaterialType->pipelineLayout, 0, 1, &getCurrentFrame().globalSet, 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentMaterialType->pipelineLayout, 1, 1, &getCurrentFrame().sceneSet, 0, nullptr);

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentMaterialType->pipeline);

			lastMaterialType = currentMaterialType;
		}

		const GPUData::PushConstants constants = {
			.drawDataIndex = i,
		};
		vkCmdPushConstants(cmd, currentMaterialType->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUData::PushConstants), &constants);

		// TODO : Find better way of handling mesh handle
		// Currently having to recreate handle which is not good.
		const RenderMesh* currentMesh { &m_meshes.get(object.meshHandle)};
		const Runic::MeshDesc* currentMeshDesc = { &currentMesh->meshDesc };
		if (currentMesh != lastMesh)
		{
			const VkDeviceSize offset{ 0 };
			const VkBuffer vertexBuffer = ResourceManager::ptr->GetBuffer(currentMesh->vertexBuffer).buffer;
			vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
			if (currentMeshDesc->hasIndices())
			{
				const VkBuffer indexBuffer = ResourceManager::ptr->GetBuffer(currentMesh->indexBuffer).buffer;
				vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
			}
			lastMesh = currentMesh;
		}

		if (currentMeshDesc->hasIndices())
		{
			vkCmdDrawIndexed(cmd, static_cast<uint32_t>(currentMeshDesc->indices.size()), 1, 0, 0, 0);
		}
		else
		{
			vkCmdDraw(cmd, static_cast<uint32_t>(currentMeshDesc->vertices.size()), 1, 0, 0);
		}
	}
}

void Renderer::draw(Camera* const camera, const std::vector<RenderObject*>& renderObjects)
{
	ZoneScoped;

	assert(renderObjects.size() < MAX_OBJECTS);

	m_currentCamera = camera;

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
	Editor::DrawEditor();

	ImGui::Render();

	VK_CHECK(vkWaitForFences(m_device, 1, &getCurrentFrame().renderFen, true, 1000000000));

	uint32_t m_swapchainImageIndex;
	VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain.swapchain, 1000000000, getCurrentFrame().presentSem, nullptr, &m_swapchainImageIndex);
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

	const VkCommandBuffer cmd = m_graphics.commands[getCurrentFrameNumber()].buffer;

	const VkCommandBufferBeginInfo cmdBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};

	vkBeginCommandBuffer(cmd, &cmdBeginInfo);

	const VkViewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(m_window->GetWidth()),
		.height = static_cast<float>(m_window->GetHeight()),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	const VkRect2D scissor{
		.offset = {.x = 0,.y = 0},
		.extent = {.width = m_window->GetWidth(), .height = m_window->GetHeight()}
	};

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	// Why src is NONE, https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#interactions-with-semaphores
	VkImageMemoryBarrier2 presentImgMemBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.image = m_swapchain.images[m_swapchainImageIndex],
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	VkImageMemoryBarrier2 renderImgMemBarrier = presentImgMemBarrier;
	renderImgMemBarrier.image = ResourceManager::ptr->GetImage(m_renderImage).image;

	const VkImageMemoryBarrier2 depthImgMemBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
		.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
		.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.image = ResourceManager::ptr->GetImage(m_depthImage).image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	VkImageMemoryBarrier2 initialBarriers[] = { presentImgMemBarrier,renderImgMemBarrier, depthImgMemBarrier};

	VkDependencyInfo presentImgDependencyInfo = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = std::size(initialBarriers),
		.pImageMemoryBarriers = initialBarriers,
	};

	vkCmdPipelineBarrier2(cmd, &presentImgDependencyInfo);

	const VkRenderingAttachmentInfo colorAttachInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = ResourceManager::ptr->GetImage(m_renderImage).imageView,
		.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = { 
			.color = {0.0f, 0.0f, 0.0f, 1.0f}
		}
	};

	const VkRenderingAttachmentInfo depthAttachInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = ResourceManager::ptr->GetImage(m_depthImage).imageView,
		.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = {
			.depthStencil = {1.0f},
		}
	};

	const VkRenderingInfo renderInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = scissor,
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachInfo,
		.pDepthAttachment = &depthAttachInfo,
	};
	vkCmdBeginRendering(cmd, &renderInfo);

	drawObjects(cmd, renderObjects);

	vkCmdEndRendering(cmd);

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

	VkImageMemoryBarrier2 shaderReadBarriers[] = { imgMemBarrier,depthShaderImgMemBarrier};

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
	VkRenderPassBeginInfo rpInfo = VulkanInit::renderpassBeginInfo(m_imguiPass, windowExtent, m_swapchain.framebuffers[m_swapchainImageIndex]);
	const VkClearValue clearValues[] = { clearValue };
	rpInfo.clearValueCount = 1;
	rpInfo.pClearValues = &clearValues[0];

	vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	vkCmdEndRenderPass(cmd);

	vkEndCommandBuffer(cmd);

	const VkPipelineStageFlags waitStage { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	const VkSubmitInfo submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &getCurrentFrame().presentSem,
		.pWaitDstStageMask = &waitStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &getCurrentFrame().renderSem,
	};

	vkQueueSubmit(m_graphics.queue, 1, &submit, getCurrentFrame().renderFen);

	const VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &getCurrentFrame().renderSem,
		.swapchainCount = 1,
		.pSwapchains = &m_swapchain.swapchain,
		.pImageIndices = &m_swapchainImageIndex,
	};
	vkQueuePresentKHR(m_graphics.queue, &presentInfo);
	FrameMark;
	m_frameNumber++;
}

void Renderer::initVulkan() {
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

void Renderer::createSwapchain()
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
	const VkFormat depthFormat{ VK_FORMAT_D32_SFLOAT };
	const VkImageCreateInfo m_depthImageInfo = VulkanInit::imageCreateInfo(depthFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, imageExtent);

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

void Renderer::destroySwapchain()
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

void Renderer::recreateSwapchain()
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


void Renderer::initImguiRenderpass() 
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

void Renderer::initImguiRenderImages()
{
	m_imguiRenderTexture = ImGui_ImplVulkan_AddTexture(m_defaultSampler, ResourceManager::ptr->GetImage(m_renderImage).imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_imguiDepthTexture = ImGui_ImplVulkan_AddTexture(m_defaultSampler, ResourceManager::ptr->GetImage(m_depthImage).imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	Editor::ViewportTexture = m_imguiRenderTexture;
	Editor::ViewportDepthTexture = m_imguiDepthTexture;
}

void Renderer::initImgui()
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

	//execute a gpu command to upload imgui font textures
	immediateSubmit([&](VkCommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
		});
	//
	////clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();
	
	m_instanceDeletionQueue.push_function([=]{
		vkDestroyDescriptorPool(m_device, imguiPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		});
}

void Renderer::initGraphicsCommands()
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

void Renderer::initComputeCommands(){
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

void Renderer::initSyncStructures()
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

void Renderer::initShaders()
{
	ZoneScoped;

	// create descriptor pool
	VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32 },
	};
	VkDescriptorPoolCreateInfo poolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 2,
		.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
		.pPoolSizes = poolSizes,
	};
	vkCreateDescriptorPool(m_device, &poolCreateInfo, nullptr, &m_globalPool);
	vkCreateDescriptorPool(m_device, &poolCreateInfo, nullptr, &m_scenePool);

	// create buffers

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		m_frame[i].drawDataBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::DrawData) * MAX_OBJECTS, .usage = GFX::Buffer::Usage::STORAGE });
		m_frame[i].transformBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::Transform) * MAX_OBJECTS, .usage = GFX::Buffer::Usage::STORAGE });
		m_frame[i].materialBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::Material) * MAX_OBJECTS, .usage = GFX::Buffer::Usage::STORAGE });

		m_frame[i].cameraBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::Camera), .usage = GFX::Buffer::Usage::UNIFORM });
		m_frame[i].dirLightBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::DirectionalLight), .usage = GFX::Buffer::Usage::UNIFORM });
	}
	// create descriptor layout

	VkDescriptorBindingFlags flags[] = {
		0,
		0,
		0,
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
	};

	VkDescriptorSetLayoutBindingFlagsCreateInfo globalBindingFlags{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(std::size(flags)),
		.pBindingFlags = flags,
	};

	const VkDescriptorSetLayoutBinding globalBindings[] = {
		{VulkanInit::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0)},
		{VulkanInit::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1)},
		{VulkanInit::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 2)},
		{VulkanInit::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3, 32)},
	};

	const VkDescriptorSetLayoutCreateInfo globalSetLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &globalBindingFlags,
		.flags = 0,
		.bindingCount = static_cast<uint32_t>(std::size(globalBindings)),
		.pBindings = globalBindings,
	};

	const VkDescriptorSetLayoutBinding sceneBindings[] = {
		{VulkanInit::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0)},
		{VulkanInit::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1)}
	};
	const VkDescriptorSetLayoutCreateInfo sceneSetLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = static_cast<uint32_t>(std::size(sceneBindings)),
		.pBindings = sceneBindings,
	};

	vkCreateDescriptorSetLayout(m_device, &globalSetLayoutInfo, nullptr, &m_globalSetLayout);
	vkCreateDescriptorSetLayout(m_device, &sceneSetLayoutInfo, nullptr, &m_sceneSetLayout);

	// create descriptors

	uint32_t counts[] = { 32 };  // Set 0 has a variable count descriptor with a maximum of 32 elements

	VkDescriptorSetVariableDescriptorCountAllocateInfo globalSetCounts = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
		.descriptorSetCount = 1,
		.pDescriptorCounts = counts
	};

	const VkDescriptorSetAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = &globalSetCounts,
		.descriptorPool = m_globalPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &m_globalSetLayout,
	};

	const VkDescriptorSetAllocateInfo sceneAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = m_scenePool,
		.descriptorSetCount = 1,
		.pSetLayouts = &m_sceneSetLayout,
	};

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		vkAllocateDescriptorSets(m_device, &allocInfo, &m_frame[i].globalSet);
		vkAllocateDescriptorSets(m_device, &sceneAllocInfo, &m_frame[i].sceneSet);

		VkDescriptorBufferInfo globalBuffers[] = {
			{.buffer = ResourceManager::ptr->GetBuffer(m_frame[i].drawDataBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(m_frame[i].drawDataBuffer).size },
			{.buffer = ResourceManager::ptr->GetBuffer(m_frame[i].transformBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(m_frame[i].transformBuffer).size},
			{.buffer = ResourceManager::ptr->GetBuffer(m_frame[i].materialBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(m_frame[i].materialBuffer).size},
		};	
		VkDescriptorBufferInfo sceneBuffers[] = {
			{.buffer = ResourceManager::ptr->GetBuffer(m_frame[i].cameraBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(m_frame[i].cameraBuffer).size},
			{.buffer = ResourceManager::ptr->GetBuffer(m_frame[i].dirLightBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(m_frame[i].dirLightBuffer).size }
		};

		const VkWriteDescriptorSet globalWrites[] = {
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_frame[i].globalSet, &globalBuffers[0], 0),
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_frame[i].globalSet, &globalBuffers[1], 1),
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_frame[i].globalSet, &globalBuffers[2], 2),
		};
		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(std::size(globalWrites)), globalWrites, 0, nullptr);
		const VkWriteDescriptorSet sceneWrites[] = {
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_frame[i].sceneSet, &sceneBuffers[0], 0),
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_frame[i].sceneSet, &sceneBuffers[1], 1)
		};
		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(std::size(sceneWrites)), sceneWrites, 0, nullptr);
	}

	// set up push constants

	const VkPushConstantRange defaultPushConstants{
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(GPUData::PushConstants),
	};

	VkDescriptorSetLayout setLayouts[] = { m_globalSetLayout, m_sceneSetLayout };
	VkPipelineLayoutCreateInfo defaultPipelineLayoutInfo = VulkanInit::pipelineLayoutCreateInfo();
	defaultPipelineLayoutInfo.setLayoutCount = 2;
	defaultPipelineLayoutInfo.pSetLayouts = setLayouts;
	defaultPipelineLayoutInfo.pushConstantRangeCount = 1;
	defaultPipelineLayoutInfo.pPushConstantRanges = &defaultPushConstants;

	VkPipelineLayout defaultPipelineLayout;
	vkCreatePipelineLayout(m_device, &defaultPipelineLayoutInfo, nullptr, &defaultPipelineLayout);

	auto shaderLoadFunc = [this](const std::string& fileLoc)->VkShaderModule {
		std::optional<VkShaderModule> shader = PipelineBuild::loadShaderModule(m_device, fileLoc.c_str());
		assert(shader.has_value());
		LOG_CORE_INFO("Shader successfully loaded" + fileLoc);
		return shader.value();
	};

	VkShaderModule vertexShader = shaderLoadFunc((std::string)"../../assets/shaders/default.vert.spv");
	VkShaderModule fragShader = shaderLoadFunc((std::string)"../../assets/shaders/default.frag.spv");

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = VulkanInit::vertexInputStateCreateInfo();
	VertexInputDescription vertexDescription = RenderMesh::getVertexDescription();
	vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexDescription.attributes.size());
	vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
	vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexDescription.bindings.size());

	// ------------------------ IMPROVE

	PipelineBuild::BuildInfo buildInfo{
		.colorBlendAttachment = VulkanInit::colorBlendAttachmentState(),
		.depthStencil = VulkanInit::depthStencilStateCreateInfo(true, true),
		.pipelineLayout = defaultPipelineLayout,
		.rasterizer = VulkanInit::rasterizationStateCreateInfo(VK_POLYGON_MODE_FILL),
		.shaderStages = {VulkanInit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader),
					VulkanInit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader)},
		.vertexInputInfo = vertexInputInfo,
	};

	VkPipeline defaultPipeline = PipelineBuild::BuildPipeline(m_device, buildInfo);	
	const std::string defaultMaterialName = "defaultMaterial";
	m_materials[defaultMaterialName] = { .pipeline = defaultPipeline, .pipelineLayout = defaultPipelineLayout };
	LOG_CORE_INFO("Material created: " + defaultMaterialName);

	VkShaderModule skyboxVertexShader = shaderLoadFunc((std::string)"../../assets/shaders/skybox.vert.spv");
	VkShaderModule skyboxFragShader = shaderLoadFunc((std::string)"../../assets/shaders/skybox.frag.spv");

	buildInfo.shaderStages = { 
		{VulkanInit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, skyboxVertexShader)},
		{VulkanInit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, skyboxFragShader)}
	};
	buildInfo.depthStencil = VulkanInit::depthStencilStateCreateInfo(true, false);

	VkPipeline skyboxPipeline = PipelineBuild::BuildPipeline(m_device, buildInfo);
	const std::string skyboxMaterialName = "skyboxMaterial";
	m_materials[skyboxMaterialName] = { .pipeline = skyboxPipeline, .pipelineLayout = defaultPipelineLayout };
	LOG_CORE_INFO("Material created: " + skyboxMaterialName);

	vkDestroyShaderModule(m_device, skyboxVertexShader, nullptr);
	vkDestroyShaderModule(m_device, skyboxFragShader, nullptr);
	vkDestroyShaderModule(m_device, vertexShader, nullptr);
	vkDestroyShaderModule(m_device, fragShader, nullptr);
}

void Renderer::deinit() 
{
	ZoneScoped;

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		vkWaitForFences(m_device, 1, &m_frame[i].renderFen, true, 1000000000);
	}

	m_instanceDeletionQueue.flush();

	destroySwapchain();

	delete ResourceManager::ptr;

	std::unordered_set<VkPipelineLayout> deletedPipelineLayouts;
	std::unordered_set<VkPipeline> deletedPipelines;
	for (auto& material : m_materials)
	{
		if (material.second.pipelineLayout != VK_NULL_HANDLE && deletedPipelineLayouts.count(material.second.pipelineLayout) == 0)
		{
			deletedPipelineLayouts.insert(material.second.pipelineLayout);
			vkDestroyPipelineLayout(m_device, material.second.pipelineLayout, nullptr);
		}
		if (material.second.pipeline != VK_NULL_HANDLE && deletedPipelines.count(material.second.pipeline) == 0)
		{
			deletedPipelines.insert(material.second.pipeline);
			vkDestroyPipeline(m_device, material.second.pipeline, nullptr);
		}
	}

	vkDestroyDescriptorPool(m_device, m_scenePool, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_sceneSetLayout, nullptr);
	vkDestroyDescriptorPool(m_device, m_globalPool, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_globalSetLayout, nullptr);

	vkDestroyFence(m_device, m_uploadContext.uploadFence, nullptr);
	vkDestroyCommandPool(m_device, m_uploadContext.commandPool, nullptr);

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		vkDestroySemaphore(m_device, m_frame[i].presentSem, nullptr);
		vkDestroySemaphore(m_device, m_frame[i].renderSem, nullptr);
		vkDestroyFence(m_device, m_frame[i].renderFen, nullptr);
	}

	vkDestroyCommandPool(m_device, m_graphics.commands[0].pool, nullptr);
	vkDestroyCommandPool(m_device, m_graphics.commands[1].pool, nullptr);
	vkDestroyCommandPool(m_device, m_compute.commands[0].pool, nullptr);

	vmaDestroyAllocator(m_allocator);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
	vkDestroyDevice(m_device, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}

Runic::MeshHandle Renderer::uploadMesh(const Runic::MeshDesc& mesh)
{
	ZoneScoped;
	RenderMesh renderMesh {.meshDesc = mesh};
	{
		const size_t bufferSize = mesh.vertices.size() * sizeof(Runic::Vertex);

		BufferHandle stagingBuffer = ResourceManager::ptr->CreateBuffer(BufferCreateInfo{
			.size = bufferSize,
			.usage = GFX::Buffer::Usage::NONE,
			.transfer = BufferCreateInfo::Transfer::SRC,
			});

		memcpy(ResourceManager::ptr->GetBuffer(stagingBuffer).ptr, mesh.vertices.data(), bufferSize);

		renderMesh.vertexBuffer = ResourceManager::ptr->CreateBuffer(BufferCreateInfo{
			.size = bufferSize,
			.usage = GFX::Buffer::Usage::VERTEX,
			.transfer = BufferCreateInfo::Transfer::DST,
			});

		const Buffer src = ResourceManager::ptr->GetBuffer(stagingBuffer);
		const Buffer dst = ResourceManager::ptr->GetBuffer(renderMesh.vertexBuffer);

		immediateSubmit([=](VkCommandBuffer cmd) {
			VkBufferCopy copy;
			copy.dstOffset = 0;
			copy.srcOffset = 0;
			copy.size = bufferSize;
			vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &copy);
			});

		ResourceManager::ptr->DestroyBuffer(stagingBuffer);
	}

	if (mesh.hasIndices())
	{
		const size_t bufferSize = mesh.indices.size() * sizeof(Runic::MeshDesc::Index);

		BufferHandle stagingBuffer = ResourceManager::ptr->CreateBuffer(BufferCreateInfo{
			.size = bufferSize,
			.usage = GFX::Buffer::Usage::INDEX,
			.transfer = BufferCreateInfo::Transfer::SRC,
			});

		memcpy(ResourceManager::ptr->GetBuffer(stagingBuffer).ptr, mesh.indices.data(), bufferSize);

		renderMesh.indexBuffer = ResourceManager::ptr->CreateBuffer(BufferCreateInfo{
			.size = bufferSize,
			.usage = GFX::Buffer::Usage::INDEX,
			.transfer = BufferCreateInfo::Transfer::DST,
			});

		const Buffer src = ResourceManager::ptr->GetBuffer(stagingBuffer);
		const Buffer dst = ResourceManager::ptr->GetBuffer(renderMesh.indexBuffer);

		immediateSubmit([=](VkCommandBuffer cmd) {
			VkBufferCopy copy;
			copy.dstOffset = 0;
			copy.srcOffset = 0;
			copy.size = bufferSize;
			vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &copy);
			});

		ResourceManager::ptr->DestroyBuffer(stagingBuffer);
	}

	return m_meshes.add(renderMesh);
}

Runic::TextureHandle Renderer::uploadTexture(const Runic::Texture& texture)
{
	if (texture.ptr == nullptr)
	{
		return Runic::TextureHandle(0);
	}

	ImageHandle newTextureHandle = (texture.m_desc.type == TextureDesc::Type::TEXTURE_CUBEMAP ? uploadTextureInternalCubemap(texture) : uploadTextureInternal(texture));
	Runic::TextureHandle bindlessHandle = m_bindlessImages.add(newTextureHandle);

	VkDescriptorImageInfo bindlessImageInfo = {
		.sampler = m_defaultSampler,
		.imageView = ResourceManager::ptr->GetImage(m_bindlessImages.get(bindlessHandle)).imageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		VkWriteDescriptorSet write{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_frame[i].globalSet,
			.dstBinding = 3,
			.dstArrayElement = bindlessHandle,
			.descriptorCount = static_cast<uint32_t>(1),
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &bindlessImageInfo,
		};

		vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
	}

	return bindlessHandle;
}

void Runic::Renderer::setSkybox(TextureHandle texture)
{
	m_skybox.textureHandle = texture;
}

ImageHandle Renderer::uploadTextureInternal(const Runic::Texture& image)
{
	const VkDeviceSize imageSize = { static_cast<VkDeviceSize>(image.texWidth * image.texHeight * 4) };
	const VkFormat image_format = {image.m_desc.format == Runic::TextureDesc::Format::DEFAULT ? DEFAULT_FORMAT : NORMAL_FORMAT };

	BufferHandle stagingBuffer = ResourceManager::ptr->CreateBuffer(BufferCreateInfo{
			.size = imageSize,
			.usage = GFX::Buffer::Usage::NONE,
			.transfer = BufferCreateInfo::Transfer::SRC,
		});

	//copy data to buffer

	memcpy(ResourceManager::ptr->GetBuffer(stagingBuffer).ptr, image.ptr[0], static_cast<size_t>(imageSize));

	const VkExtent3D imageExtent{
		.width = static_cast<uint32_t>(image.texWidth),
		.height = static_cast<uint32_t>(image.texHeight),
		.depth = 1,
	};

	const VkImageCreateInfo dimg_info = VulkanInit::imageCreateInfo(image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

	ImageHandle newImage = ResourceManager::ptr->CreateImage(ImageCreateInfo{ .imageInfo = dimg_info, .imageType = ImageCreateInfo::ImageType::TEXTURE_2D });

	immediateSubmit([&](VkCommandBuffer cmd) {
		const VkImageSubresourceRange range{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		};

		const VkImageMemoryBarrier2 imageBarrier_toTransfer{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.image = ResourceManager::ptr->GetImage(newImage).image,
			.subresourceRange = range,
		};

		VkDependencyInfo imgDependencyInfo = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &imageBarrier_toTransfer,
		};

		vkCmdPipelineBarrier2(cmd, &imgDependencyInfo);

		const VkBufferImageCopy copyRegion = {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1},
			.imageExtent = imageExtent,
		};

		vkCmdCopyBufferToImage(cmd, ResourceManager::ptr->GetBuffer(stagingBuffer).buffer, ResourceManager::ptr->GetImage(newImage).image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		const VkImageMemoryBarrier2 imageBarrier_toReadable{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.image = ResourceManager::ptr->GetImage(newImage).image,
			.subresourceRange = range,
		};

		VkDependencyInfo imgRedableDependencyInfo = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &imageBarrier_toReadable,
		};

		vkCmdPipelineBarrier2(cmd, &imgRedableDependencyInfo);
	});

	ResourceManager::ptr->DestroyBuffer(stagingBuffer);

	return newImage;
}

ImageHandle Renderer::uploadTextureInternalCubemap(const Runic::Texture& image)
{
	const VkDeviceSize imageSize = { static_cast<VkDeviceSize>(image.texSize) };
	const VkFormat image_format = { image.m_desc.format == Runic::TextureDesc::Format::DEFAULT ? DEFAULT_FORMAT : NORMAL_FORMAT };

	BufferHandle stagingBuffer = ResourceManager::ptr->CreateBuffer(BufferCreateInfo{
			.size = imageSize * 6,
			.usage = GFX::Buffer::Usage::NONE,
			.transfer = BufferCreateInfo::Transfer::SRC,
		});

	//copy data to buffer
	void* pixels[6] = {
		image.ptr[0],
		image.ptr[1],
		image.ptr[2],
		image.ptr[3],
		image.ptr[4],
		image.ptr[5],
	};

	for (int i = 0; i < 6; ++i)
	{
		memcpy(static_cast<char*>(ResourceManager::ptr->GetBuffer(stagingBuffer).ptr) + (i * imageSize), pixels[i], static_cast<size_t>(image.texSize));
	}

	const VkExtent3D imageExtent{
		.width = static_cast<uint32_t>(image.texWidth),
		.height = static_cast<uint32_t>(image.texHeight),
		.depth = 1,
	};

	const VkImageCreateInfo dimg_info = VulkanInit::imageCreateInfo(image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 6);

	ImageHandle newImage = ResourceManager::ptr->CreateImage(ImageCreateInfo{ .imageInfo = dimg_info, .imageType = ImageCreateInfo::ImageType::TEXTURE_CUBEMAP });

	immediateSubmit([&](VkCommandBuffer cmd) {
		const VkImageSubresourceRange range{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 6,
		};

		const VkImageMemoryBarrier2 imageBarrier_toTransfer{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.image = ResourceManager::ptr->GetImage(newImage).image,
			.subresourceRange = range,
		};

		VkDependencyInfo imgDependencyInfo = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &imageBarrier_toTransfer,
		};

		vkCmdPipelineBarrier2(cmd, &imgDependencyInfo);

		const VkBufferImageCopy copyRegion = {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 6},
			.imageExtent = imageExtent,
		};

		vkCmdCopyBufferToImage(cmd, ResourceManager::ptr->GetBuffer(stagingBuffer).buffer, ResourceManager::ptr->GetImage(newImage).image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		const VkImageMemoryBarrier2 imageBarrier_toReadable{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.image = ResourceManager::ptr->GetImage(newImage).image,
			.subresourceRange = range,
		};

		VkDependencyInfo imgRedableDependencyInfo = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &imageBarrier_toReadable,
		};

		vkCmdPipelineBarrier2(cmd, &imgRedableDependencyInfo);
		});

	ResourceManager::ptr->DestroyBuffer(stagingBuffer);

	return newImage;
}


void Renderer::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
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

VertexInputDescription RenderMesh::getVertexDescription()
{
	VertexInputDescription description;

	const VkVertexInputBindingDescription mainBinding = {
		.binding = 0,
		.stride = sizeof(Runic::Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	description.bindings.push_back(mainBinding);

	const VkVertexInputAttributeDescription positionAttribute = {
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Runic::Vertex, position),
	};

	const VkVertexInputAttributeDescription normalAttribute = {
		.location = 1,
		.binding = 0,
		.format = NORMAL_FORMAT,
		.offset = offsetof(Runic::Vertex, normal),
	};

	const VkVertexInputAttributeDescription colorAttribute = {
		.location = 2,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Runic::Vertex, color),
	};

	const VkVertexInputAttributeDescription uvAttribute = {
		.location = 3,
		.binding = 0,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = offsetof(Runic::Vertex, uv),
	};

	description.attributes.push_back(positionAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(colorAttribute);
	description.attributes.push_back(uvAttribute);
	return description;
}
