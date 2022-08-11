#pragma once


#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

namespace PipelineBuild {
	std::optional<VkShaderModule> loadShaderModule(VkDevice device,const char* filePath);

	struct BuildInfo
	{
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		VkPipelineLayout pipelineLayout = {};
		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {};
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	};

	VkPipeline BuildPipeline(VkDevice device, const BuildInfo& pipelineBuildInfo);
};
