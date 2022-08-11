#pragma once
#include "Common.h"
#include "vulkan/vulkan.h"

namespace VkCommon
{
	inline constexpr VkShaderStageFlags ToVulkan(const GFX::Stages stages)
	{
		VkShaderStageFlags bits;

		if (stages && GFX::Stages::VERTEX) bits |= VK_SHADER_STAGE_VERTEX_BIT;
		if (stages && GFX::Stages::FRAGMENT) bits |= VK_SHADER_STAGE_FRAGMENT_BIT;
		if (stages && GFX::Stages::ALL) bits |= VK_SHADER_STAGE_ALL;

		return bits;
	}

	inline constexpr VkBufferUsageFlags ToVulkan(const GFX::Buffer::Usage usage)
	{
		switch (usage)
		{
		default:
			return 0;
		case GFX::Buffer::Usage::NONE:
			return 0;
		case GFX::Buffer::Usage::UNIFORM:
			return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		case GFX::Buffer::Usage::STORAGE:
			return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		case GFX::Buffer::Usage::VERTEX:
			return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		case GFX::Buffer::Usage::INDEX:
			return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		}
	}
}
