#include "Runic/Graphics/Renderer.h"

#include <vk_mem_alloc.h>

#include <vulkan/vulkan.h>

#include <Tracy.hpp>
#include <common/TracySystem.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/transform.hpp>
#include <gtx/quaternion.hpp>

#include <iostream>
#include <memory>
#include <unordered_set>
#include <stb_image.h>

#include "Runic/Graphics/Internal/VulkanInit.h"
#include "Runic/Log.h"

#include "Runic/Scene/Components/TransformComponent.h"

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


void Renderer::Init(Device* device)
{
	ZoneScoped;

	m_graphicsDevice = device;


	initShaders();
	initShaderData();
}

void Renderer::initShaderData()
{
	ZoneScoped;

	m_skybox.meshHandle = UploadMesh(MeshDesc::GenerateSkyboxCube());
	m_skybox.textureHandle = 0;
}

void Renderer::drawObjects(VkCommandBuffer cmd, const std::vector<Runic::Entity*>& renderObjects)
{
	ZoneScoped;
	const int COUNT = static_cast<int>(renderObjects.size());
	const Runic::RenderableComponent* FIRST = &renderObjects[0]->GetComponent<RenderableComponent>();

	// fill buffers
	// binding 0
		//slot 0 - transform
	GPUData::DrawData* drawDataSSBO = (GPUData::DrawData*)ResourceManager::ptr->GetBuffer(GetCurrentFrame().drawDataBuffer).ptr;
	GPUData::Transform* objectSSBO = (GPUData::Transform*)ResourceManager::ptr->GetBuffer(GetCurrentFrame().transformBuffer).ptr;
	GPUData::Material* materialSSBO = (GPUData::Material*)ResourceManager::ptr->GetBuffer(GetCurrentFrame().materialBuffer).ptr;

	for (int i = 0; i < COUNT; ++i)
	{
		int bufferPos = i + 1;
		const Runic::RenderableComponent& object = renderObjects[i]->GetComponent<RenderableComponent>();

		if (renderObjects[i]->HasComponent<TransformComponent>())
		{
			const Runic::TransformComponent& transform = renderObjects[i]->GetComponent<TransformComponent>();

			const glm::mat4 modelMatrix = transform.BuildMatrix();
			objectSSBO[bufferPos].modelMatrix = modelMatrix;
			objectSSBO[bufferPos].normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

			drawDataSSBO[i].transformIndex = bufferPos;
		}

		drawDataSSBO[i].materialIndex = bufferPos;

		materialSSBO[bufferPos] = GPUData::Material{
			.specular = {0.4f,0.4,0.4f},
			.shininess = 64.0f,
			.textureIndices = {object.textureHandle.has_value() ? object.textureHandle.value() : -1,
							object.normalHandle.has_value() ? object.normalHandle.value() : -1,
							object.roughnessHandle.has_value() ? object.roughnessHandle.value() : -1,
							object.emissionHandle.has_value() ? object.emissionHandle.value() : -1},
		};
	}

	int skyboxCount = COUNT + 1;
	drawDataSSBO[COUNT].transformIndex = skyboxCount;
	drawDataSSBO[COUNT].materialIndex = skyboxCount;

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
	GPUData::Camera* cameraSSBO = (GPUData::Camera*)ResourceManager::ptr->GetBuffer(GetCurrentFrame().cameraBuffer).ptr;
	cameraSSBO->view = m_currentCamera->BuildViewMatrix();
	cameraSSBO->proj = m_currentCamera->BuildProjMatrix();
	cameraSSBO->pos = {m_currentCamera->GetPosition(), 0.0f};
		//slot 1 - directionalLight
	GPUData::DirectionalLight* dirLightSSBO = (GPUData::DirectionalLight*)ResourceManager::ptr->GetBuffer(GetCurrentFrame().dirLightBuffer).ptr;
	if (m_entities_with_lights.count(LightComponent::LightType::Directional) > 0)
	{
		const LightComponent* dirLight = &m_entities_with_lights[LightComponent::LightType::Directional][0]->GetComponent<LightComponent>();
		dirLightSSBO->ambient = { dirLight->ambient, 0.0f };
		dirLightSSBO->diffuse = { dirLight->diffuse, 0.0f };
		dirLightSSBO->specular = { dirLight->specular, 0.0f };
		dirLightSSBO->direction = { dirLight->direction, 0.0f };
	}
	else
	{
		LOG_CORE_WARN("No directional light passed to renderer.");
		*dirLightSSBO = GPUData::DirectionalLight();
	}
	GPUData::PointLight* pointLightSSBO = (GPUData::PointLight*)ResourceManager::ptr->GetBuffer(GetCurrentFrame().pointLightBuffer).ptr;
	if (m_entities_with_lights.count(LightComponent::LightType::Point) > 0)
	{
		if (int numberOfPointLights = m_entities_with_lights[LightComponent::LightType::Point].size(); numberOfPointLights > 0)
		{
			for (int i = 0; i < numberOfPointLights; ++i)
			{
				const LightComponent* pointLight = &m_entities_with_lights[LightComponent::LightType::Point][i]->GetComponent<LightComponent>();
				pointLightSSBO[i].ambient = { pointLight->ambient, 0.0f };
				pointLightSSBO[i].diffuse = { pointLight->diffuse, 0.0f };
				pointLightSSBO[i].specular = { pointLight->specular, 0.0f };
				pointLightSSBO[i].position = { pointLight->position, 0.0f };

				pointLightSSBO[i].constant = pointLight->constant;
				pointLightSSBO[i].linear =  pointLight->linear;
				pointLightSSBO[i].quadratic = pointLight->quadratic;
			}
		}
	}

	const MaterialType* lastMaterialType = nullptr;
	const RenderMesh* lastMesh = nullptr;
	for (int i = 0; i < COUNT + 1; ++i)
	{
		if (i < COUNT && !renderObjects[i]->HasComponent<RenderableComponent>())
		{
			return;
		}
		const Runic::RenderableComponent& object = i != COUNT ? renderObjects[i]->GetComponent<RenderableComponent>() : m_skybox;

		// TODO : RenderObjects hold material handle for different m_materials
		const MaterialType* currentMaterialType{ i != COUNT ? &m_materials["defaultMaterial"] : &m_materials["skyboxMaterial"]};
		if (currentMaterialType != lastMaterialType)
		{
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentMaterialType->pipelineLayout, 0, 1, &GetCurrentFrame().globalSet, 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentMaterialType->pipelineLayout, 1, 1, &GetCurrentFrame().sceneSet, 0, nullptr);

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsDevice->m_pipelineManager->GetPipeline(currentMaterialType->pipeline));

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

void Renderer::Draw(Camera* const camera)
{
	ZoneScoped;

	m_currentCamera = camera;

	if (!m_graphicsDevice->BeginFrame())
	{
		return;
	}

	const VkCommandBuffer cmd = m_graphicsDevice->m_graphics.commands[m_graphicsDevice->GetCurrentFrameNumber()].buffer;

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
		.width = static_cast<float>(m_graphicsDevice->m_window->GetWidth()),
		.height = static_cast<float>(m_graphicsDevice->m_window->GetHeight()),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	const VkRect2D scissor{
		.offset = {.x = 0,.y = 0},
		.extent = {.width = m_graphicsDevice->m_window->GetWidth(), .height = m_graphicsDevice->m_window->GetHeight()}
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
		.image = m_graphicsDevice->GetBackBuffer().image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	VkImageMemoryBarrier2 renderImgMemBarrier = presentImgMemBarrier;

	const VkImageMemoryBarrier2 depthImgMemBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
		.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
		.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.image = ResourceManager::ptr->GetImage(m_graphicsDevice->GetDepthImage()).image,
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
		.imageView = m_graphicsDevice->GetBackBuffer().imageView,
		.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = { 
			.color = {0.0f, 0.0f, 0.0f, 1.0f}
		}
	};

	const VkRenderingAttachmentInfo depthAttachInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = ResourceManager::ptr->GetImage(m_graphicsDevice->GetDepthImage()).imageView,
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

	drawObjects(cmd, m_entities);

	vkCmdEndRendering(cmd);

	m_graphicsDevice->AddImGuiToCommandBuffer();

	vkEndCommandBuffer(cmd);

	const VkPipelineStageFlags waitStage { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	const VkSubmitInfo submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_graphicsDevice->GetCurrentFrame().presentSem,
		.pWaitDstStageMask = &waitStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_graphicsDevice->GetCurrentFrame().renderSem,
	};

	vkQueueSubmit(m_graphicsDevice->m_graphics.queue, 1, &submit, m_graphicsDevice->GetCurrentFrame().renderFen);

	m_graphicsDevice->EndFrame();
	m_graphicsDevice->Present();
}

void Runic::Renderer::GiveRenderables(const std::vector<std::shared_ptr<Runic::Entity>>& entities)
{
	for (const auto& entity : entities){
		if (entity->HasComponent<RenderableComponent>())
		{	
			m_entities.push_back(entity.get());
		}

		if (entity->HasComponent<LightComponent>())
		{
			m_entities_with_lights[entity->GetComponent<LightComponent>().lightType].push_back(entity.get());
		}
	}
}

void Renderer::initShaders()
{
	ZoneScoped;

	// create descriptor pool
	VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES },
	};
	VkDescriptorPoolCreateInfo poolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 2,
		.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
		.pPoolSizes = poolSizes,
	};
	vkCreateDescriptorPool(m_graphicsDevice->m_device, &poolCreateInfo, nullptr, &m_globalPool);
	vkCreateDescriptorPool(m_graphicsDevice->m_device, &poolCreateInfo, nullptr, &m_scenePool);

	// create buffers

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		m_frame[i].drawDataBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::DrawData) * MAX_OBJECTS, .usage = GFX::Buffer::Usage::STORAGE });
		m_frame[i].transformBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::Transform) * MAX_OBJECTS, .usage = GFX::Buffer::Usage::STORAGE });
		m_frame[i].materialBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::Material) * MAX_OBJECTS, .usage = GFX::Buffer::Usage::STORAGE });

		m_frame[i].cameraBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::Camera), .usage = GFX::Buffer::Usage::UNIFORM });
		m_frame[i].dirLightBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::DirectionalLight), .usage = GFX::Buffer::Usage::UNIFORM });
		m_frame[i].pointLightBuffer = ResourceManager::ptr->CreateBuffer({ .size = sizeof(GPUData::PointLight) * MAX_POINT_LIGHTS, .usage = GFX::Buffer::Usage::STORAGE });
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
		{VulkanInit::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3, MAX_TEXTURES)},
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
		{VulkanInit::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1)},
		{VulkanInit::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 2)}
	};
	const VkDescriptorSetLayoutCreateInfo sceneSetLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = static_cast<uint32_t>(std::size(sceneBindings)),
		.pBindings = sceneBindings,
	};

	vkCreateDescriptorSetLayout(m_graphicsDevice->m_device, &globalSetLayoutInfo, nullptr, &m_globalSetLayout);
	vkCreateDescriptorSetLayout(m_graphicsDevice->m_device, &sceneSetLayoutInfo, nullptr, &m_sceneSetLayout);

	// create descriptors

	uint32_t counts[] = { MAX_TEXTURES };  // Set 0 has a variable count descriptor with a maximum of 32 elements

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
		vkAllocateDescriptorSets(m_graphicsDevice->m_device, &allocInfo, &m_frame[i].globalSet);
		vkAllocateDescriptorSets(m_graphicsDevice->m_device, &sceneAllocInfo, &m_frame[i].sceneSet);

		VkDescriptorBufferInfo globalBuffers[] = {
			{.buffer = ResourceManager::ptr->GetBuffer(m_frame[i].drawDataBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(m_frame[i].drawDataBuffer).size },
			{.buffer = ResourceManager::ptr->GetBuffer(m_frame[i].transformBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(m_frame[i].transformBuffer).size},
			{.buffer = ResourceManager::ptr->GetBuffer(m_frame[i].materialBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(m_frame[i].materialBuffer).size},
		};	
		VkDescriptorBufferInfo sceneBuffers[] = {
			{.buffer = ResourceManager::ptr->GetBuffer(m_frame[i].cameraBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(m_frame[i].cameraBuffer).size},
			{.buffer = ResourceManager::ptr->GetBuffer(m_frame[i].dirLightBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(m_frame[i].dirLightBuffer).size },
			{.buffer = ResourceManager::ptr->GetBuffer(m_frame[i].pointLightBuffer).buffer, .range = ResourceManager::ptr->GetBuffer(m_frame[i].pointLightBuffer).size }
		};

		const VkWriteDescriptorSet globalWrites[] = {
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_frame[i].globalSet, &globalBuffers[0], 0),
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_frame[i].globalSet, &globalBuffers[1], 1),
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_frame[i].globalSet, &globalBuffers[2], 2),
		};
		vkUpdateDescriptorSets(m_graphicsDevice->m_device, static_cast<uint32_t>(std::size(globalWrites)), globalWrites, 0, nullptr);
		const VkWriteDescriptorSet sceneWrites[] = {
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_frame[i].sceneSet, &sceneBuffers[0], 0),
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_frame[i].sceneSet, &sceneBuffers[1], 1),
			VulkanInit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_frame[i].sceneSet, &sceneBuffers[2], 2)
		};
		vkUpdateDescriptorSets(m_graphicsDevice->m_device, static_cast<uint32_t>(std::size(sceneWrites)), sceneWrites, 0, nullptr);
	}

	// set up push constants

	const VkPushConstantRange defaultPushConstants{
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(GPUData::PushConstants),
	};
	std::vector<VkPushConstantRange> pushConstants = { defaultPushConstants };
	std::vector<VkDescriptorSetLayout> setLayouts = { m_globalSetLayout, m_sceneSetLayout };

	VkPipelineLayout defaultPipelineLayout = m_graphicsDevice->m_pipelineManager->CreatePipelineLayout(setLayouts, pushConstants);

	const std::string defaultMaterialName = "defaultMaterial";
	PipelineHandle defaultMat = m_graphicsDevice->m_pipelineManager->CreatePipeline({
		.name = defaultMaterialName,
		.pipelineLayout = defaultPipelineLayout,
		.vertexShader = "../../assets/shaders/default.vert.spv",
		.fragmentShader = "../../assets/shaders/default.frag.spv",
		.vertexInputDesc = RenderMesh::getVertexDescription(),
		.colourFormat = m_graphicsDevice->GetBackBufferImageFormat(),
		.depthFormat = DEPTH_FORMAT,
	});
	m_materials[defaultMaterialName] = { .pipeline = defaultMat, .pipelineLayout = defaultPipelineLayout };
	LOG_CORE_INFO("Material created: " + defaultMaterialName);

	const std::string skyboxMaterialName = "skyboxMaterial";
	PipelineHandle skyboxMat = m_graphicsDevice->m_pipelineManager->CreatePipeline({
		.name = skyboxMaterialName,
		.pipelineLayout = defaultPipelineLayout,
		.vertexShader = "../../assets/shaders/skybox.vert.spv",
		.fragmentShader = "../../assets/shaders/skybox.frag.spv",
		.vertexInputDesc = RenderMesh::getVertexDescription(),
		.enableDepthWrite = false,
		.colourFormat = m_graphicsDevice->GetBackBufferImageFormat(),
		.depthFormat = DEPTH_FORMAT,
		});
	m_materials[skyboxMaterialName] = { .pipeline = skyboxMat, .pipelineLayout = defaultPipelineLayout };
	LOG_CORE_INFO("Material created: " + skyboxMaterialName);
}

void Renderer::Deinit() 
{
	ZoneScoped;

	m_graphicsDevice->WaitIdle();

	vkDestroyDescriptorPool(m_graphicsDevice->m_device, m_scenePool, nullptr);
	vkDestroyDescriptorSetLayout(m_graphicsDevice->m_device, m_sceneSetLayout, nullptr);
	vkDestroyDescriptorPool(m_graphicsDevice->m_device, m_globalPool, nullptr);
	vkDestroyDescriptorSetLayout(m_graphicsDevice->m_device, m_globalSetLayout, nullptr);
}

Runic::MeshHandle Renderer::UploadMesh(const Runic::MeshDesc& mesh)
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

		m_graphicsDevice->ImmediateSubmit([=](VkCommandBuffer cmd) {
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

		m_graphicsDevice->ImmediateSubmit([=](VkCommandBuffer cmd) {
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

Runic::TextureHandle Renderer::UploadTexture(const Runic::Texture& texture)
{
	if (texture.ptr == nullptr)
	{
		return Runic::TextureHandle(0);
	}

	const ImageHandle newTextureHandle = (texture.m_desc.type == TextureDesc::Type::TEXTURE_CUBEMAP ? uploadTextureInternalCubemap(texture) : uploadTextureInternal(texture));
	Runic::TextureHandle bindlessHandle = m_bindlessImages.add(newTextureHandle);

	const VkDescriptorImageInfo bindlessImageInfo = {
		.sampler = m_graphicsDevice->m_defaultSampler,
		.imageView = ResourceManager::ptr->GetImage(m_bindlessImages.get(bindlessHandle)).imageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	for (int i = 0; i < FRAME_OVERLAP; ++i)
	{
		const VkWriteDescriptorSet write{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_frame[i].globalSet,
			.dstBinding = 3,
			.dstArrayElement = bindlessHandle,
			.descriptorCount = static_cast<uint32_t>(1),
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &bindlessImageInfo,
		};

		vkUpdateDescriptorSets(m_graphicsDevice->m_device, 1, &write, 0, nullptr);
	}

	return bindlessHandle;
}

void Runic::Renderer::SetSkybox(TextureHandle texture)
{
	m_skybox.textureHandle = texture;
}

ImageHandle Renderer::uploadTextureInternal(const Runic::Texture& image)
{
	assert(image.ptr != nullptr);

	const VkDeviceSize imageSize = { static_cast<VkDeviceSize>(image.texWidth * image.texHeight * 4) };
	const VkFormat image_format = {image.m_desc.format == Runic::TextureDesc::Format::DEFAULT ? DEFAULT_FORMAT : NORMAL_FORMAT };

	const BufferHandle stagingBuffer = ResourceManager::ptr->CreateBuffer(BufferCreateInfo{
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

	m_graphicsDevice->ImmediateSubmit([&](VkCommandBuffer cmd) {
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
	void* pixels[6] = {
		image.ptr[0],
		image.ptr[1],
		image.ptr[2],
		image.ptr[3],
		image.ptr[4],
		image.ptr[5],
	};

	assert(image.ptr[0] != nullptr);
	assert(image.ptr[1] != nullptr);
	assert(image.ptr[2] != nullptr);
	assert(image.ptr[3] != nullptr);
	assert(image.ptr[4] != nullptr);
	assert(image.ptr[5] != nullptr);

	const VkDeviceSize imageSize = { static_cast<VkDeviceSize>(image.texSize) };
	const VkFormat image_format = { image.m_desc.format == Runic::TextureDesc::Format::DEFAULT ? DEFAULT_FORMAT : NORMAL_FORMAT };

	const BufferHandle stagingBuffer = ResourceManager::ptr->CreateBuffer(BufferCreateInfo{
			.size = imageSize * 6,
			.usage = GFX::Buffer::Usage::NONE,
			.transfer = BufferCreateInfo::Transfer::SRC,
		});

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

	m_graphicsDevice->ImmediateSubmit([&](VkCommandBuffer cmd) {
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

	description.attributes.push_back(positionAttribute);

	const VkVertexInputAttributeDescription normalAttribute = {
		.location = 1,
		.binding = 0,
		.format = NORMAL_FORMAT,
		.offset = offsetof(Runic::Vertex, normal),
	};

	description.attributes.push_back(normalAttribute);

	const VkVertexInputAttributeDescription colorAttribute = {
		.location = 2,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Runic::Vertex, color),
	};

	description.attributes.push_back(colorAttribute);

	const VkVertexInputAttributeDescription uvAttribute = {
		.location = 3,
		.binding = 0,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = offsetof(Runic::Vertex, uv),
	};

	description.attributes.push_back(uvAttribute);

	const VkVertexInputAttributeDescription tangentAttribute = {
		.location = 4,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Runic::Vertex, tangent),
	};

	description.attributes.push_back(tangentAttribute);

	return description;
}
