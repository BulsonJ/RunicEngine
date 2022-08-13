#include "Runic/Graphics/Internal/PipelineBuilder.h"
#include <assert.h>
#include <fstream>

std::optional<VkShaderModule> PipelineBuild::loadShaderModule(VkDevice device, const char* filePath){
	//open the file. With cursor at the end
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		return std::nullopt;
	}

	size_t fileSize = (size_t)file.tellg();

	//spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire file
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	file.seekg(0);
	file.read((char*)buffer.data(), fileSize);
	file.close();


	VkShaderModuleCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,
		.codeSize = buffer.size() * sizeof(uint32_t),
		.pCode = buffer.data()
	};

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		return std::nullopt;
	}
	return shaderModule;
	}

VkPipeline PipelineBuild::BuildPipeline(VkDevice device, const BuildInfo& buildInfo)
{
	assert(buildInfo.shaderStages.empty() == false);
	assert(buildInfo.pipelineLayout != VK_NULL_HANDLE);

	const VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr,
	};

	const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	const VkPipelineDynamicStateCreateInfo dynamicStateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.flags = 0,
		.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size()),
		.pDynamicStates = dynamicStateEnables.data(),
	};

	const VkPipelineColorBlendStateCreateInfo colorBlending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &buildInfo.colorBlendAttachment,
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = nullptr,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	// ----- NOT USED -----
	const VkPipelineTessellationStateCreateInfo tessState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
	};

	const VkPipelineMultisampleStateCreateInfo sampleState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};

	// ----- NOT USED -----

	VkPipelineRenderingCreateInfo dynamicRenderingInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	VkFormat colorAttachmentFormats[] = { VK_FORMAT_R8G8B8A8_SRGB };

	dynamicRenderingInfo.pNext = nullptr;
	dynamicRenderingInfo.colorAttachmentCount = std::size(colorAttachmentFormats);
	dynamicRenderingInfo.pColorAttachmentFormats = colorAttachmentFormats;
	dynamicRenderingInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
	dynamicRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const VkGraphicsPipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &dynamicRenderingInfo,
		.stageCount = static_cast<uint32_t>(buildInfo.shaderStages.size()),
		.pStages = buildInfo.shaderStages.data(),
		.pVertexInputState = &buildInfo.vertexInputInfo,
		.pInputAssemblyState = &inputAssemblyCreateInfo,
		.pTessellationState = &tessState,
		.pViewportState = &viewportState,
		.pRasterizationState = &buildInfo.rasterizer,
		.pMultisampleState = &sampleState,
		.pDepthStencilState = &buildInfo.depthStencil,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicStateInfo,
		.layout = buildInfo.pipelineLayout,
		.renderPass = nullptr,
		.basePipelineHandle = VK_NULL_HANDLE,
	};

	VkPipeline newPipeline;
	VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline);
	if (result != VK_SUCCESS)
	{
		//assert(newPipeline != VK_NULL_HANDLE);
		return VK_NULL_HANDLE; // failed to create graphics pipeline
	}
	return newPipeline;
}
