#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace Runic
{
	typedef uint32_t PipelineHandle;

	struct VertexInputDescription
	{
		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attributes;
		VkPipelineVertexInputStateCreateFlags flags = 0;
	};

	struct PipelineInfo
	{
		std::string name;
		VkPipeline pipeline{};
		VkPipelineLayout pipelineLayout{};

		std::string vertexShader;
		std::string fragmentShader;

		VertexInputDescription vertexInputDesc;
		bool enableDepthWrite{ true };
		VkFormat colourFormat;
		VkFormat depthFormat;
	};

	class PipelineManager
	{
	public:
		PipelineManager(VkDevice device) : m_device(device) {}
		void Deinit();
		VkPipeline GetPipeline(PipelineHandle handle);
		VkPipelineLayout CreatePipelineLayout(std::vector<VkDescriptorSetLayout> descLayouts,
			std::vector<VkPushConstantRange> pushConstants);
		PipelineHandle CreatePipeline(PipelineInfo info);
		void RecompilePipelines();
	private:
		VkPipeline createPipelineInternal(PipelineInfo info);
		const VkDevice m_device;
		std::string m_sourceFolder;
		std::vector<VkPipelineLayout> m_pipelineLayouts;
		std::vector<PipelineInfo> m_pipelines;
	};
}

