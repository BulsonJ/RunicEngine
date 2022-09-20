#include "Runic/Graphics/ResourceManager.h"
#include "Runic/Graphics/Internal/VulkanInit.h"
#include "Runic/Graphics/Internal/VulkanCommon.h"

void ResourceManager::Deinit()
{
	for (const auto& buffer : buffers.m_array)
	{
		if (buffer.buffer != VK_NULL_HANDLE)
		{
			vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
		}
	}

	for (const auto& image : images.m_array)
	{
		if (image.image != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device, image.imageView, nullptr);
			vmaDestroyImage(allocator, image.image, image.allocation);
		}
	}
}

Buffer ResourceManager::GetBuffer(const BufferHandle& buffer)
{
	return buffers.get(buffer);
}

BufferHandle ResourceManager::CreateBuffer(const BufferCreateInfo& createInfo)
{
	VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = createInfo.size,
	};
	bufferInfo.usage = VkCommon::ToVulkan(createInfo.usage);

	switch (createInfo.transfer)
	{
	default:
		break;
	case BufferCreateInfo::Transfer::NONE:
		break;
	case BufferCreateInfo::Transfer::SRC:
		bufferInfo.usage = bufferInfo.usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		break;
	case BufferCreateInfo::Transfer::DST:
		bufferInfo.usage = bufferInfo.usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		break;
	}

	VmaAllocationCreateInfo vmaallocInfo = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	Buffer newBuffer;

	//allocate the buffer
	vmaCreateBuffer(allocator, &bufferInfo, &vmaallocInfo,
		&newBuffer.buffer,
		&newBuffer.allocation,
		nullptr);

	VmaAllocationInfo allocInfo;
	vmaGetAllocationInfo(allocator, newBuffer.allocation, &allocInfo);
	newBuffer.ptr = allocInfo.pMappedData;
	newBuffer.size = createInfo.size;

	BufferHandle newHandle = buffers.add(newBuffer);

	return newHandle;
}

void ResourceManager::DestroyBuffer(const BufferHandle& buffer)
{
	Buffer& deleteBuffer = buffers.get(buffer);
	if (deleteBuffer.buffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(allocator, deleteBuffer.buffer, deleteBuffer.allocation);
	}
	deleteBuffer.buffer = VK_NULL_HANDLE;
}

ImageHandle ResourceManager::CreateImage(const ImageCreateInfo& createInfo)
{
	Image newImage;

	VmaAllocationCreateInfo allocInfo = {
		.usage = VMA_MEMORY_USAGE_AUTO ,
	};

	vmaCreateImage(allocator, &createInfo.imageInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr);

	const VkImageAspectFlags imageViewType = createInfo.usage == ImageCreateInfo::Usage::COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;

	VkImageViewCreateInfo imageinfo = {};
	switch (createInfo.imageType)
	{
	case ImageCreateInfo::ImageType::TEXTURE_2D:
		imageinfo = VulkanInit::imageViewCreateInfo(createInfo.imageInfo.format, newImage.image, imageViewType);
		break;
	case ImageCreateInfo::ImageType::TEXTURE_CUBEMAP:
		imageinfo = VulkanInit::imageViewCreateInfo(createInfo.imageInfo.format, newImage.image, imageViewType, VK_IMAGE_VIEW_TYPE_CUBE, 6);
		break;
	default:
		break;
	}
	
	switch (createInfo.usage)
	{
	default:
		break;
	case ImageCreateInfo::Usage::DEPTH:
		imageinfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageinfo.components.g = VK_COMPONENT_SWIZZLE_R;
		imageinfo.components.b = VK_COMPONENT_SWIZZLE_R;
		break;
	}

	vkCreateImageView(device, &imageinfo, nullptr, &newImage.imageView);

	ImageHandle newHandle = images.add(newImage);

	return newHandle;
}

Image ResourceManager::GetImage(const ImageHandle& image)
{
	return images.get(image);
}

void ResourceManager::DestroyImage(const ImageHandle& image)
{
	Image& deleteImage = images.get(image);
	if (deleteImage.image != VK_NULL_HANDLE)
	{
		vmaDestroyImage(allocator, deleteImage.image, deleteImage.allocation);
		vkDestroyImageView(device, deleteImage.imageView, nullptr);
	}
	deleteImage.image = VK_NULL_HANDLE;
}


