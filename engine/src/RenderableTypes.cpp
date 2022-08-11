#include "RenderableTypes.h"

#include <Tracy.hpp>
#include "Log.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

RenderableTypes::Texture::~Texture()
{
	stbi_image_free(this->ptr);
}

void RenderableTypes::TextureUtil::LoadTextureFromFile(const char* file, RenderableTypes::TextureDesc textureDesc, RenderableTypes::Texture& outImage)
{
	ZoneScoped;

	stbi_uc* pixels = stbi_load(file, &outImage.texWidth, &outImage.texHeight, &outImage.texChannels, STBI_rgb_alpha);

	if (!pixels)
	{
		LOG_CORE_WARN("Failed to load texture file: " + *file);
		return;
	}

	outImage.desc = textureDesc;
	outImage.ptr = pixels;

	return;
}
