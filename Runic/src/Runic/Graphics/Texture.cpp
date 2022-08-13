#include "Runic/Graphics/Texture.h"

#include <Tracy.hpp>
#include "Runic/Log.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <string_view>

using namespace Runic;

Texture::~Texture()
{
	stbi_image_free(this->ptr);
}

void TextureUtil::LoadTextureFromFile(const char* file, TextureDesc textureDesc, Texture& outImage)
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

	LOG_CORE_INFO(file);

	return;
}