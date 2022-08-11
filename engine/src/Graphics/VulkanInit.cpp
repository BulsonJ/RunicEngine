#include "Graphics/VulkanInit.h"

VkCommandPoolCreateInfo VulkanInit::commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags /*= 0*/)
{
	VkCommandPoolCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,

		.flags = flags,
		.queueFamilyIndex = queueFamilyIndex,
	};
	return info;
}

VkCommandBufferAllocateInfo VulkanInit::commandBufferAllocateInfo(VkCommandPool pool, uint32_t count /*= 1*/, VkCommandBufferLevel level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/)
{
	VkCommandBufferAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,

		.commandPool = pool,
		.level = level,
		.commandBufferCount = count,

	};
	return info;
}

VkCommandBufferBeginInfo VulkanInit::commandBufferBeginInfo(VkCommandBufferUsageFlags flags /*= 0*/)
{
	VkCommandBufferBeginInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,

		.flags = flags,
		.pInheritanceInfo = nullptr,

	};
	return info;
}

VkSubmitInfo VulkanInit::submitInfo(VkCommandBuffer* cmd)
{
	VkSubmitInfo info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,

		.waitSemaphoreCount = 0,
		.pWaitSemaphores = nullptr,
		.pWaitDstStageMask = nullptr,
		.commandBufferCount = 1,
		.pCommandBuffers = cmd,
		.signalSemaphoreCount = 0,
		.pSignalSemaphores = nullptr,
	};

	return info;
}

VkPipelineShaderStageCreateInfo VulkanInit::pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule)
{

	VkPipelineShaderStageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.pNext = nullptr;

	info.stage = stage;
	info.module = shaderModule;
	info.pName = "main";
	return info;
}

VkPipelineVertexInputStateCreateInfo VulkanInit::vertexInputStateCreateInfo()
{
	VkPipelineVertexInputStateCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,

		.vertexBindingDescriptionCount = 0,
		.vertexAttributeDescriptionCount = 0,
	};
	return info;
}

VkPipelineRasterizationStateCreateInfo VulkanInit::rasterizationStateCreateInfo(VkPolygonMode polygonMode)
{
	VkPipelineRasterizationStateCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = nullptr,

		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,

		.polygonMode = polygonMode,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f,
	};

	return info;
}

VkPipelineMultisampleStateCreateInfo VulkanInit::multisamplingStateCreateInfo()
{
	VkPipelineMultisampleStateCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,

		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};
	return info;
}

VkPipelineColorBlendAttachmentState VulkanInit::colorBlendAttachmentState()
{
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};
	return colorBlendAttachment;
}

VkPipelineLayoutCreateInfo VulkanInit::pipelineLayoutCreateInfo()
{
	VkPipelineLayoutCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,

		.flags = 0,
		.setLayoutCount = 0,
		.pSetLayouts = nullptr,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr,
	};
	return info;
}

VkPipelineDepthStencilStateCreateInfo VulkanInit::depthStencilStateCreateInfo(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp /* = VK_COMPARE_OP_LESS_OR_EQUAL*/)
{
	VkPipelineDepthStencilStateCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,

		.depthTestEnable = bDepthTest ? VK_TRUE : VK_FALSE,
		.depthWriteEnable = bDepthWrite ? VK_TRUE : VK_FALSE,
		.depthCompareOp = bDepthTest ? compareOp : VK_COMPARE_OP_ALWAYS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f,
	};

	return info;
}

VkFenceCreateInfo VulkanInit::fenceCreateInfo(VkFenceCreateFlags flags)
{
	VkFenceCreateInfo fenceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = flags,
	};
	return fenceCreateInfo;
}

VkSemaphoreCreateInfo VulkanInit::semaphoreCreateInfo(VkSemaphoreCreateFlags flags)
{
	VkSemaphoreCreateInfo semCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = flags,
	};
	return semCreateInfo;
}

VkImageCreateInfo VulkanInit::imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, VkImageCreateFlags createFlags, uint32_t arrayLayerCount, VkImageType imageType)
{
	VkImageCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,

		.flags = createFlags,
		.imageType = imageType,

		.format = format,
		.extent = extent,

		.mipLevels = 1,
		.arrayLayers = arrayLayerCount,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usageFlags,
	};

	return info;
}

VkImageViewCreateInfo VulkanInit::imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags, VkImageViewType imageType, uint32_t arrayLayerCount)
{
	//build a image-view for the depth image to use for rendering
	VkImageViewCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,

		.image = image,
		.viewType = imageType,
		.format = format,
	};

	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = arrayLayerCount;
	info.subresourceRange.aspectMask = aspectFlags;

	return info;
}


VkWriteDescriptorSet VulkanInit::writeDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding)
{
	VkWriteDescriptorSet write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,

		.dstSet = dstSet,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = type,
		.pBufferInfo = bufferInfo,
	};

	return write;
}

VkWriteDescriptorSet VulkanInit::writeDescriptorImage(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding)
{
	VkWriteDescriptorSet write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,

		.dstSet = dstSet,
		.dstBinding = binding,

		.descriptorCount = 1,
		.descriptorType = type,
		.pImageInfo = imageInfo,
	};

	return write;
}

VkDescriptorSetLayoutBinding VulkanInit::descriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding, uint32_t descriptorCount /*= 1*/)
{
	VkDescriptorSetLayoutBinding setbind = {
		.binding = binding,
		.descriptorType = type,
		.descriptorCount = descriptorCount,
		.stageFlags = stageFlags,
		.pImmutableSamplers = nullptr,
	};

	return setbind;
}

VkSamplerCreateInfo VulkanInit::samplerCreateInfo(VkFilter filters, VkSamplerAddressMode samplerAddressMode /*= VK_SAMPLER_ADDRESS_MODE_REPEAT*/)
{
	VkSamplerCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,

		.magFilter = filters,
		.minFilter = filters,
		.addressModeU = samplerAddressMode,
		.addressModeV = samplerAddressMode,
		.addressModeW = samplerAddressMode,
	};

	return info;
}

VkRenderPassBeginInfo VulkanInit::renderpassBeginInfo(VkRenderPass renderPass, VkExtent2D windowExtent, VkFramebuffer framebuffer)
{
	VkRenderPassBeginInfo info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = nullptr,

		.renderPass = renderPass,
		.framebuffer = framebuffer,
		.clearValueCount = 1,
		.pClearValues = nullptr,

	};

	info.renderArea.offset.x = 0;
	info.renderArea.offset.y = 0;
	info.renderArea.extent = windowExtent;

	return info;
}