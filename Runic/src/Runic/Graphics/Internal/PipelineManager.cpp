#include "Runic/Graphics/Internal/PipelineManager.h"

#include <optional>

#include "Runic/Graphics/Internal/VulkanInit.h"
#include "Runic/Graphics/Internal/PipelineBuilder.h"
#include "Runic/Log.h"
#include "Runic/Graphics/Device.h"

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


void PipelineManager::Deinit()
{
	for (const VkPipelineLayout layout : m_pipelineLayouts)
	{
		vkDestroyPipelineLayout(m_device, layout, nullptr);
	}
	for (const PipelineInfo pipeline : m_pipelines)
	{
		vkDestroyPipeline(m_device, pipeline.pipeline, nullptr);
	}
}

VkPipeline PipelineManager::GetPipeline(PipelineHandle handle)
{
	return m_pipelines.at(handle).pipeline;
}

VkPipelineLayout PipelineManager::CreatePipelineLayout(std::vector<VkDescriptorSetLayout> descLayouts, std::vector<VkPushConstantRange> pushConstants)
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = VulkanInit::pipelineLayoutCreateInfo();
	pipelineLayoutInfo.setLayoutCount = descLayouts.size();
	pipelineLayoutInfo.pSetLayouts = descLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = pushConstants.size();
	pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();
	VkPipelineLayout pipelineLayout{};
	VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &pipelineLayout));
	m_pipelineLayouts.push_back(pipelineLayout);
	return pipelineLayout;
}

PipelineHandle PipelineManager::CreatePipeline(PipelineInfo info)
{
	uint32_t index = (uint32_t)m_pipelines.size();
	m_pipelines.push_back(info);
	m_pipelines.back().pipeline = createPipelineInternal(info);
	return index;
}

void PipelineManager::RecompilePipelines()
{
	for (PipelineInfo& info : m_pipelines)
	{
		vkDestroyPipeline(m_device, info.pipeline, nullptr);

		info.pipeline = createPipelineInternal(info);
	}
}

VkPipeline PipelineManager::createPipelineInternal(PipelineInfo info)
{
	auto shaderLoadFunc = [this](const std::string& fileLoc)->VkShaderModule {
		std::optional<VkShaderModule> shader = PipelineBuild::loadShaderModule(m_device, fileLoc.c_str());
		assert(shader.has_value());
		LOG_CORE_INFO("Shader successfully loaded" + fileLoc);
		return shader.value();
	};

	VkShaderModule vertexShader = shaderLoadFunc(info.vertexShader);
	VkShaderModule fragShader = shaderLoadFunc(info.fragmentShader);

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = VulkanInit::vertexInputStateCreateInfo();;
	vertexInputInfo.pVertexAttributeDescriptions = info.vertexInputDesc.attributes.data();
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(info.vertexInputDesc.attributes.size());
	vertexInputInfo.pVertexBindingDescriptions = info.vertexInputDesc.bindings.data();
	vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(info.vertexInputDesc.bindings.size());

	PipelineBuild::BuildInfo buildInfo{
		.colorBlendAttachment = VulkanInit::colorBlendAttachmentState(),
		.depthStencil = VulkanInit::depthStencilStateCreateInfo(true, info.enableDepthWrite),
		.pipelineLayout = info.pipelineLayout,
		.rasterizer = VulkanInit::rasterizationStateCreateInfo(VK_POLYGON_MODE_FILL),
		.shaderStages = {VulkanInit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader),
					VulkanInit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader)},
		.vertexInputInfo = vertexInputInfo,
		.colorAttachmentFormats = {info.colourFormat},
		.depthAttachmentFormat = {info.depthFormat}
	};

	VkPipeline pipeline = PipelineBuild::BuildPipeline(m_device, buildInfo);

	vkDestroyShaderModule(m_device, vertexShader, nullptr);
	vkDestroyShaderModule(m_device, fragShader, nullptr);
	return pipeline;
}
