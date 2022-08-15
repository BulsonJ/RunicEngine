#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <array>

#include "Runic/Graphics/Common.h"
#include "Runic/Structures/Slotmap.h"


struct BufferCreateInfo
{
	std::size_t size;

	GFX::Buffer::Usage usage {GFX::Buffer::Usage::NONE};

	enum class Transfer
	{
		NONE,
		SRC,
		DST
	} transfer {Transfer::NONE};
};

struct ImageCreateInfo
{
	VkImageCreateInfo imageInfo;
	enum class ImageType
	{
		TEXTURE_2D,
		TEXTURE_CUBEMAP
	} imageType;
	enum class Usage
	{
		COLOR,
		DEPTH
	} usage;
};

struct Buffer
{
	VkBuffer buffer{ VK_NULL_HANDLE };
	VmaAllocation allocation{};
	void* ptr {nullptr};
	std::size_t size{};
};

struct Image
{
	VkImage image { VK_NULL_HANDLE };
	VmaAllocation allocation{};
	VkImageView imageView{ VK_NULL_HANDLE };
};

typedef uint32_t BufferHandle;
typedef uint32_t ImageHandle;

class ResourceManager
{
public:
	static ResourceManager* ptr;
	ResourceManager(const VkDevice device, const VmaAllocator allocator) : device(device), allocator(allocator) {}
	~ResourceManager();

	BufferHandle CreateBuffer(const BufferCreateInfo& createInfo);
	Buffer GetBuffer(const BufferHandle& buffer);
	void DestroyBuffer(const BufferHandle& buffer);

	ImageHandle CreateImage(const ImageCreateInfo& createInfo);
	Image GetImage(const ImageHandle& image);
	void DestroyImage(const ImageHandle& image);
protected:
	const VkDevice device;
	const VmaAllocator allocator;

	Slotmap<Buffer> buffers;
	Slotmap<Image> images;
};

