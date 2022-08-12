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

#include "Runic/Graphics/VulkanInit.h"
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


void Renderer::init()
{
	ZoneScoped;

	SDL_Init(SDL_INIT_VIDEO);

	const SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	window.window = SDL_CreateWindow(
		"Runic Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		window.extent.width,
		window.extent.height,
		window_flags
	);

	initVulkan();

	createSwapchain();
	initGraphicsCommands();
	initComputeCommands();
	initSyncStructures();

	initImguiRenderpass();
	initImgui();
	initImguiRenderImages();


	initShaders();

	initShaderData();

}

void Renderer::initShaderData()
{
	ZoneScoped;
	camera.proj = glm::perspective(glm::radians(90.0f), 800.0f / 600.0f, 0.1f, 100.0f);
	camera.proj[1][1] *= -1;
	camera.pos = { 6.0f,3.0f,6.0f,0.0f };
	Editor::lightDirection = &sunlight.direction;
	Editor::lightColor = &sunlight.color;
	Editor::lightAmbientColor = &sunlight.ambientColor;
}

void Renderer::drawObjects(VkCommandBuffer cmd, const std::vector<std::shared_ptr<RenderObject>>& renderObjects)
{	
	ZoneScoped;
	const int COUNT = static_cast<int>(renderObjects.size());
	const Runic::RenderObject* FIRST = renderObjects.data()->get();

	// fill buffers
	// binding 0
		//slot 0 - transform
	GPUData::DrawData* drawDataSSBO = (GPUData::DrawData*)ResourceManager::ptr->GetBuffer(getCurrentFrame().drawDataBuffer).ptr;
	GPUData::Transform* objectSSBO = (GPUData::Transform*)ResourceManager::ptr->GetBuffer(getCurrentFrame().transformBuffer).ptr;
	GPUData::Material* materialSSBO = (GPUData::Material*)ResourceManager::ptr->GetBuffer(getCurrentFrame().materialBuffer).ptr;

	for (int i = 0; i < COUNT; ++i)
	{
		const Runic::RenderObject& object = *renderObjects[i].get();

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
	// binding 1
		//slot 0 - camera
	camera.view =
		glm::lookAt({ camera.pos.x,camera.pos.y,camera.pos.z },
			glm::vec3(0.0f, -0.5f, 0.0f),
			UP_DIR);
	//const float rotationSpeed = 0.5f;
	//camera.view = glm::rotate(camera.view, (frameNumber / 120.0f) * rotationSpeed, UP_DIR);
	GPUData::Camera* cameraSSBO = (GPUData::Camera*)ResourceManager::ptr->GetBuffer(getCurrentFrame().cameraBuffer).ptr;
	*cameraSSBO = camera;
		//slot 1 - directionalLight
	GPUData::DirectionalLight* dirLightSSBO = (GPUData::DirectionalLight*)ResourceManager::ptr->GetBuffer(getCurrentFrame().dirLightBuffer).ptr;
	*dirLightSSBO = sunlight;

	const MaterialType* lastMaterialType = nullptr;
	const RenderMesh* lastMesh = nullptr;
	for (int i = 0; i < COUNT; ++i)
	{
		const Runic::RenderObject& object = *renderObjects[i].get();

		// TODO : RenderObjects hold material handle for different materials
		const MaterialType* currentMaterialType{ &materials["defaultMaterial"] };
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
		const RenderMesh* currentMesh { &meshes.get(object.meshHandle)};
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

void Renderer::draw(const std::vector<std::shared_ptr<RenderObject>>& renderObjects)
{
	ZoneScoped;

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame(window.window);
	ImGui::NewFrame();

	Editor::ViewportTexture = imguiRenderTexture[getCurrentFrameNumber()];
	Editor::DrawEditor();

	ImGui::Render();

	VK_CHECK(vkWaitForFences(device, 1, &getCurrentFrame().renderFen, true, 1000000000));

	uint32_t swapchainImageIndex;
	VkResult result = vkAcquireNextImageKHR(device, swapchain.swapchain, 1000000000, getCurrentFrame().presentSem, nullptr, &swapchainImageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.resized == true)
	{
		window.resized = false;
		recreateSwapchain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	VK_CHECK(vkResetFences(device, 1, &getCurrentFrame().renderFen));
	VK_CHECK(vkResetCommandBuffer(graphics.commands[getCurrentFrameNumber()].buffer, 0));

	const VkCommandBuffer cmd = graphics.commands[getCurrentFrameNumber()].buffer;

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
		.width = static_cast<float>(window.extent.width),
		.height = static_cast<float>(window.extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	const VkRect2D scissor{
		.offset = {.x = 0,.y = 0},
		.extent = window.extent
	};

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	const VkImageMemoryBarrier presentImgMemBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.image = swapchain.images[swapchainImageIndex],
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	VkImageMemoryBarrier renderImgMemBarrier = presentImgMemBarrier;
	renderImgMemBarrier.image = ResourceManager::ptr->GetImage(getCurrentFrame().renderImage).image;

	VkImageMemoryBarrier initialBarriers[] = { presentImgMemBarrier,renderImgMemBarrier };

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		static_cast<uint32_t>(std::size(initialBarriers)),
		initialBarriers
	);

	const VkImageMemoryBarrier depthImgMemBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.image = ResourceManager::ptr->GetImage(depthImage).image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&depthImgMemBarrier
	);

	const VkRenderingAttachmentInfo colorAttachInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = ResourceManager::ptr->GetImage(getCurrentFrame().renderImage).imageView,
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = { 
			.color = {0.0f, 0.0f, 0.0f, 1.0f}
		}
	};

	const VkRenderingAttachmentInfo depthAttachInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = ResourceManager::ptr->GetImage(depthImage).imageView,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
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

	const VkImageMemoryBarrier imgMemBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.image = ResourceManager::ptr->GetImage(getCurrentFrame().renderImage).image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};
		
	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&imgMemBarrier
	);


	const VkImageMemoryBarrier depthShaderImgMemBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.image = ResourceManager::ptr->GetImage(depthImage).image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&depthShaderImgMemBarrier
	);
	Editor::ViewportTexture = imguiRenderTexture[getCurrentFrameNumber()];

	const VkClearValue clearValue{
		.color = { 0.1f, 0.1f, 0.1f, 1.0f }
	};
	VkRenderPassBeginInfo rpInfo = VulkanInit::renderpassBeginInfo(imguiPass, window.extent, swapchain.framebuffers[swapchainImageIndex]);
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

	vkQueueSubmit(graphics.queue, 1, &submit, getCurrentFrame().renderFen);

	const VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &getCurrentFrame().renderSem,
		.swapchainCount = 1,
		.pSwapchains = &swapchain.swapchain,
		.pImageIndices = &swapchainImageIndex,
	};
	vkQueuePresentKHR(graphics.queue, &presentInfo);
	FrameMark;
	frameNumber++;
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

	instance = vkb_inst.instance;
	debugMessenger = vkb_inst.debug_messenger;

	SDL_Vulkan_CreateSurface(window.window, instance, &surface);

	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	const vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 2)
		.set_surface(surface)
		.select()
		.value();

	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
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

	const vkb::Device vkbDevice = deviceBuilder
		.add_pNext(&descIndexFeatures)
		.build()
		.value();

	device = vkbDevice.device;
	chosenGPU = physicalDevice.physical_device;
	gpuProperties = vkbDevice.physical_device.properties;

	graphics.queue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	graphics.queueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
	compute.queue = vkbDevice.get_queue(vkb::QueueType::compute).value();
	compute.queueFamily = vkbDevice.get_queue_index(vkb::QueueType::compute).value();

	const VmaAllocatorCreateInfo allocatorInfo = {
		.physicalDevice = chosenGPU,
		.device = device,
		.instance = instance,
	};
	vmaCreateAllocator(&allocatorInfo, &allocator);

	ResourceManager::ptr = new ResourceManager(device, allocator);
	LOG_CORE_INFO("Vulkan Initialised");
}

void Renderer::createSwapchain()
{
	ZoneScoped;
	vkb::SwapchainBuilder swapchainBuilder{ chosenGPU,device,surface };

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(window.extent.width, window.extent.height)
		.build()
		.value();

	swapchain.swapchain = vkbSwapchain.swapchain;
	swapchain.images = vkbSwapchain.get_images().value();
	swapchain.imageViews = vkbSwapchain.get_image_views().value();
	swapchain.imageFormat = vkbSwapchain.image_format;

	const VkDeviceSize imageSize = { static_cast<VkDeviceSize>(window.extent.height * window.extent.width * 4) };
	const VkFormat image_format{ VK_FORMAT_R8G8B8A8_SRGB };

	const VkExtent3D imageExtent{
		.width = static_cast<uint32_t>(window.extent.width),
		.height = static_cast<uint32_t>(window.extent.height),
		.depth = 1,
	};

	const VkImageCreateInfo imageInfo = VulkanInit::imageCreateInfo(image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, imageExtent);
	const VkFormat depthFormat{ VK_FORMAT_D32_SFLOAT };
	const VkImageCreateInfo depthImageInfo = VulkanInit::imageCreateInfo(depthFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, imageExtent);
	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		frame[i].renderImage = ResourceManager::ptr->CreateImage(ImageCreateInfo{
			.imageInfo = imageInfo,
			.imageType = ImageCreateInfo::ImageType::TEXTURE_2D,
			.usage = ImageCreateInfo::Usage::COLOR
			});

	}

	depthImage = ResourceManager::ptr->CreateImage(ImageCreateInfo{
			.imageInfo = depthImageInfo,
			.imageType = ImageCreateInfo::ImageType::TEXTURE_2D,
			.usage = ImageCreateInfo::Usage::DEPTH
		});
	LOG_CORE_INFO("Create Swapchain");
}

void Renderer::destroySwapchain()
{
	for (int i = 0; i < swapchain.imageViews.size(); i++)
	{
		vkDestroyFramebuffer(device, swapchain.framebuffers[i], nullptr);
		vkDestroyImageView(device, swapchain.imageViews[i], nullptr);
	}
	vkDestroyRenderPass(device, imguiPass, nullptr);
	vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		ResourceManager::ptr->DestroyImage(frame[i].renderImage);
	};
	LOG_CORE_INFO("Destroy swapchain");
}

void Renderer::recreateSwapchain()
{
	ZoneScoped;
	
	SDL_Event e;
	int flags = SDL_GetWindowFlags(window.window);
	bool minimized = (flags & SDL_WINDOW_MINIMIZED) ? true : false;
	while (minimized)
	{
		flags = SDL_GetWindowFlags(window.window);
		minimized = (flags & SDL_WINDOW_MINIMIZED) ? true : false;
		SDL_WaitEvent(&e);
	}
	int width = 0, height = 0;
	SDL_GetWindowSize(window.window, &width, &height);

	vkDeviceWaitIdle(device);

	window.extent.width = width;
	window.extent.height = height;
	destroySwapchain();
	createSwapchain();
	initImguiRenderpass();
	initImguiRenderImages();
}


void Renderer::initImguiRenderpass() 
{
	const VkAttachmentDescription color_attachment = {
		.format = swapchain.imageFormat,
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
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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

	VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &imguiPass));

	VkFramebufferCreateInfo fb_info = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext = nullptr,
		.renderPass = imguiPass,
		.attachmentCount = 1,
		.width = window.extent.width,
		.height = window.extent.height,
		.layers = 1,
	};

	const int swapchain_imagecount = static_cast<int>(swapchain.images.size());
	swapchain.framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

	//create framebuffers for each of the swapchain image views
	for (int i = 0; i < swapchain_imagecount; i++)
	{

		VkImageView attachment = swapchain.imageViews[i];

		fb_info.pAttachments = &attachment;
		fb_info.attachmentCount = 1;

		VK_CHECK(vkCreateFramebuffer(device, &fb_info, nullptr, &swapchain.framebuffers[i]));
	}
}

void Renderer::initImguiRenderImages()
{
	VkSamplerCreateInfo samplerInfo = VulkanInit::samplerCreateInfo(VK_FILTER_NEAREST);

	VkSampler imageSampler;
	vkCreateSampler(device, &samplerInfo, nullptr, &imageSampler);
	instanceDeletionQueue.push_function([=] {
		vkDestroySampler(device, imageSampler, nullptr);
	});

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		imguiRenderTexture[i] = ImGui_ImplVulkan_AddTexture(imageSampler, ResourceManager::ptr->GetImage(frame[i].renderImage).imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	imguiDepthTexture = ImGui_ImplVulkan_AddTexture(imageSampler, ResourceManager::ptr->GetImage(depthImage).imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	Editor::ViewportDepthTexture = imguiDepthTexture;
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
	VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool));
	
	ImGui::CreateContext();
	
	ImGui_ImplSDL2_InitForVulkan(window.window);
	
	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {
		.Instance = instance,
		.PhysicalDevice = chosenGPU,
		.Device = device,
		.Queue = graphics.queue,
		.DescriptorPool = imguiPool,
		.MinImageCount = 3,
		.ImageCount = 3,
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
	};
	
	ImGui_ImplVulkan_Init(&init_info, imguiPass);
	
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
	
	instanceDeletionQueue.push_function([=]{
		vkDestroyDescriptorPool(device, imguiPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		});
}

void Renderer::initGraphicsCommands()
{
	ZoneScoped;
	const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = graphics.queueFamily
	};

	for (int i = 0; i < 2; ++i)
	{
		VkCommandPool* commandPool = &graphics.commands[i].pool;

		vkCreateCommandPool(device, &graphicsCommandPoolCreateInfo, nullptr, commandPool);

		const VkCommandBufferAllocateInfo bufferAllocInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = *commandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		vkAllocateCommandBuffers(device, &bufferAllocInfo, &graphics.commands[i].buffer);
	}


	const VkCommandPoolCreateInfo uploadCommandPoolInfo = VulkanInit::commandPoolCreateInfo(graphics.queueFamily);
	vkCreateCommandPool(device, &uploadCommandPoolInfo, nullptr, &uploadContext.commandPool);

	const VkCommandBufferAllocateInfo cmdAllocInfo = VulkanInit::commandBufferAllocateInfo(uploadContext.commandPool, 1);
	vkAllocateCommandBuffers(device, &cmdAllocInfo, &uploadContext.commandBuffer);
}

void Renderer::initComputeCommands(){
	ZoneScoped;
	const VkCommandPoolCreateInfo computeCommandPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = compute.queueFamily
	};

	VkCommandPool* commandPool = &compute.commands[0].pool;

	vkCreateCommandPool(device, &computeCommandPoolCreateInfo, nullptr, commandPool);

	const VkCommandBufferAllocateInfo bufferAllocInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = *commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	vkAllocateCommandBuffers(device, &bufferAllocInfo, &compute.commands[0].buffer);
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

		vkCreateFence(device, &fenceCreateInfo, nullptr, &frame[i].renderFen);

		const VkSemaphoreCreateInfo semaphoreCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0
		};

		vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame[i].presentSem);
		vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame[i].renderSem);
	}


	const VkFenceCreateInfo uploadFenceCreateInfo = VulkanInit::fenceCreateInfo();
	vkCreateFence(device, &uploadFenceCreateInfo, nullptr, &uploadContext.uploadFence);


}

void Renderer::initShaders()
{

	ZoneScoped;

	// create descriptor pool
	VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32 },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 10 }
	};
	VkDescriptorPoolCreateInfo poolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 2,
		.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
		.pPoolSizes = poolSizes,
	};
	vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &globalPool);
	vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &scenePool);

	// create buffers

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		frame[i].drawDataBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::DrawData) * MAX_OBJECTS, .usage = GFX::Buffer::Usage::STORAGE });
		frame[i].transformBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::Transform) * MAX_OBJECTS, .usage = GFX::Buffer::Usage::STORAGE });
		frame[i].materialBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::Material) * MAX_OBJECTS, .usage = GFX::Buffer::Usage::STORAGE });

		frame[i].cameraBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::Camera), .usage = GFX::Buffer::Usage::UNIFORM });
		frame[i].dirLightBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::DirectionalLight), .usage = GFX::Buffer::Usage::UNIFORM });
	}
	// create descriptor layout

	VkDescriptorBindingFlags flags[] = {
		0,
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
		{VulkanInit::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3)},
		{VulkanInit::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 4, 32)},
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

	vkCreateDescriptorSetLayout(device, &globalSetLayoutInfo, nullptr, &globalSetLayout);
	vkCreateDescriptorSetLayout(device, &sceneSetLayoutInfo, nullptr, &sceneSetLayout);

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
		.descriptorPool = globalPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &globalSetLayout,
	};

	const VkDescriptorSetAllocateInfo sceneAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = scenePool,
		.descriptorSetCount = 1,
		.pSetLayouts = &sceneSetLayout,
	};

	VkSamplerCreateInfo samplerInfo = VulkanInit::samplerCreateInfo(VK_FILTER_NEAREST);
	VkSampler imageSampler;
	vkCreateSampler(device, &samplerInfo, nullptr, &imageSampler);
	instanceDeletionQueue.push_function([=] {
		vkDestroySampler(device, imageSampler, nullptr);
		});

	VkDescriptorImageInfo samplerDescInfo{.sampler = imageSampler };

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		vkAllocateDescriptorSets(device, &allocInfo, &frame[i].globalSet);
		vkAllocateDescriptorSets(device, &sceneAllocInfo, &frame[i].sceneSet);

		VkDescriptorBufferInfo globalBuffers[] = {
			{.buffer = ResourceManager::ptr->GetBuffer(frame[i].drawDataBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(frame[i].drawDataBuffer).size },
			{.buffer = ResourceManager::ptr->GetBuffer(frame[i].transformBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(frame[i].transformBuffer).size},
			{.buffer = ResourceManager::ptr->GetBuffer(frame[i].materialBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(frame[i].materialBuffer).size},
		};	
		VkDescriptorBufferInfo sceneBuffers[] = {
			{.buffer = ResourceManager::ptr->GetBuffer(frame[i].cameraBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(frame[i].cameraBuffer).size},
			{.buffer = ResourceManager::ptr->GetBuffer(frame[i].dirLightBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(frame[i].dirLightBuffer).size }
		};

		const VkWriteDescriptorSet globalWrites[] = {
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frame[i].globalSet, &globalBuffers[0], 0),
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frame[i].globalSet, &globalBuffers[1], 1),
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frame[i].globalSet, &globalBuffers[2], 2),
			VulkanInit::writeDescriptorImage(VK_DESCRIPTOR_TYPE_SAMPLER, frame[i].globalSet, &samplerDescInfo, 3)
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(std::size(globalWrites)), globalWrites, 0, nullptr);
		const VkWriteDescriptorSet sceneWrites[] = {
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frame[i].sceneSet, &sceneBuffers[0], 0),
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frame[i].sceneSet, &sceneBuffers[1], 1)
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(std::size(sceneWrites)), sceneWrites, 0, nullptr);
	}

	// set up push constants

	const VkPushConstantRange defaultPushConstants{
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(GPUData::PushConstants),
	};

	VkDescriptorSetLayout setLayouts[] = { globalSetLayout, sceneSetLayout };
	VkPipelineLayoutCreateInfo defaultPipelineLayoutInfo = VulkanInit::pipelineLayoutCreateInfo();
	defaultPipelineLayoutInfo.setLayoutCount = 2;
	defaultPipelineLayoutInfo.pSetLayouts = setLayouts;
	defaultPipelineLayoutInfo.pushConstantRangeCount = 1;
	defaultPipelineLayoutInfo.pPushConstantRanges = &defaultPushConstants;

	VkPipelineLayout defaultPipelineLayout;
	vkCreatePipelineLayout(device, &defaultPipelineLayoutInfo, nullptr, &defaultPipelineLayout);

	auto shaderLoadFunc = [this](const std::string& fileLoc)->VkShaderModule {
		std::optional<VkShaderModule> shader = PipelineBuild::loadShaderModule(device, fileLoc.c_str());
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

	VkPipeline defaultPipeline = PipelineBuild::BuildPipeline(device, buildInfo);	
	const std::string defaultMaterialName = "defaultMaterial";
	materials[defaultMaterialName] = { .pipeline = defaultPipeline, .pipelineLayout = defaultPipelineLayout };
	LOG_CORE_INFO("Material created: " + defaultMaterialName);

	vkDestroyShaderModule(device, vertexShader, nullptr);
	vkDestroyShaderModule(device, fragShader, nullptr);
}

void Renderer::deinit() 
{
	ZoneScoped;

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		vkWaitForFences(device, 1, &frame[i].renderFen, true, 1000000000);
	}

	instanceDeletionQueue.flush();

	destroySwapchain();

	delete ResourceManager::ptr;

	for (auto& material : materials)
	{
		vkDestroyPipelineLayout(device, material.second.pipelineLayout, nullptr);
		vkDestroyPipeline(device, material.second.pipeline, nullptr);
	}

	vkDestroyDescriptorPool(device, scenePool, nullptr);
	vkDestroyDescriptorSetLayout(device, sceneSetLayout, nullptr);
	vkDestroyDescriptorPool(device, globalPool, nullptr);
	vkDestroyDescriptorSetLayout(device, globalSetLayout, nullptr);

	vkDestroyFence(device, uploadContext.uploadFence, nullptr);
	vkDestroyCommandPool(device, uploadContext.commandPool, nullptr);

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		vkDestroySemaphore(device, frame[i].presentSem, nullptr);
		vkDestroySemaphore(device, frame[i].renderSem, nullptr);
		vkDestroyFence(device, frame[i].renderFen, nullptr);
	}

	vkDestroyCommandPool(device, graphics.commands[0].pool, nullptr);
	vkDestroyCommandPool(device, graphics.commands[1].pool, nullptr);
	vkDestroyCommandPool(device, compute.commands[0].pool, nullptr);

	vmaDestroyAllocator(allocator);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkb::destroy_debug_utils_messenger(instance, debugMessenger);
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);

	SDL_DestroyWindow(window.window);
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

	LOG_CORE_INFO("Mesh Uploaded");
	return meshes.add(renderMesh);
}

Runic::TextureHandle Renderer::uploadTexture(const Runic::Texture& texture)
{
	if (texture.ptr == nullptr)
	{
		return Runic::TextureHandle(0);
	}
	ImageHandle newTextureHandle = uploadTextureInternal(texture);
	Runic::TextureHandle bindlessHandle = bindlessImages.add(newTextureHandle);

	VkDescriptorImageInfo bindlessImageInfo = {
		.imageView = ResourceManager::ptr->GetImage(bindlessImages.get(bindlessHandle)).imageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		VkWriteDescriptorSet write{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = frame[i].globalSet,
			.dstBinding = 4,
			.dstArrayElement = bindlessHandle,
			.descriptorCount = static_cast<uint32_t>(1),
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.pImageInfo = &bindlessImageInfo,
		};

		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	}

	LOG_CORE_INFO("Texture Uploaded: ");

	return bindlessHandle;
}

ImageHandle Renderer::uploadTextureInternal(const Runic::Texture& image)
{
	const VkDeviceSize imageSize = { static_cast<VkDeviceSize>(image.texWidth * image.texHeight * 4) };
	const VkFormat image_format = {image.desc.format == Runic::TextureDesc::Format::DEFAULT ? DEFAULT_FORMAT : NORMAL_FORMAT };

	BufferHandle stagingBuffer = ResourceManager::ptr->CreateBuffer(BufferCreateInfo{
			.size = imageSize,
			.usage = GFX::Buffer::Usage::NONE,
			.transfer = BufferCreateInfo::Transfer::SRC,
		});

	//copy data to buffer

	memcpy(ResourceManager::ptr->GetBuffer(stagingBuffer).ptr, image.ptr, static_cast<size_t>(imageSize));

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

		const VkImageMemoryBarrier imageBarrier_toTransfer = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.image = ResourceManager::ptr->GetImage(newImage).image,
			.subresourceRange = range,
		};

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toTransfer);

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

		VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;

		imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toReadable);
		});

	ResourceManager::ptr->DestroyBuffer(stagingBuffer);

	return newImage;
}

void Renderer::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	VkCommandBuffer cmd = uploadContext.commandBuffer;

	VkCommandBufferBeginInfo cmdBeginInfo = VulkanInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	vkBeginCommandBuffer(cmd, &cmdBeginInfo);

	function(cmd);

	vkEndCommandBuffer(cmd);

	VkSubmitInfo submit = VulkanInit::submitInfo(&cmd);

	vkQueueSubmit(graphics.queue, 1, &submit, uploadContext.uploadFence);

	vkWaitForFences(device, 1, &uploadContext.uploadFence, true, 9999999999);
	vkResetFences(device, 1, &uploadContext.uploadFence);

	vkResetCommandPool(device, uploadContext.commandPool, 0);
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
