#include "Runic/Graphics/Texture.h"

#include <Tracy.hpp>
#include "Runic/Log.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <string_view>

using namespace Runic;

Texture::~Texture()
{
	switch (this->m_desc.type)
	{
	default:
		break;
	case TextureDesc::Type::TEXTURE_2D:
		stbi_image_free(this->ptr[0]);
		break;
	case TextureDesc::Type::TEXTURE_CUBEMAP:
		for (int i = 0; i < 6; i++)
		{
			stbi_image_free(this->ptr[i]);
		}
		break;
	}
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

	outImage.m_desc = textureDesc;
	outImage.ptr[0] = pixels;

	return;
}

void Runic::TextureUtil::LoadCubemapFromFile(const char* file[6], TextureDesc textureDesc, Texture& outImage)
{
	ZoneScoped;

	stbi_uc* pixels[6] = { 
		stbi_load(file[0], &outImage.texWidth, &outImage.texHeight, &outImage.texChannels, STBI_rgb_alpha),
		stbi_load(file[1], &outImage.texWidth, &outImage.texHeight, &outImage.texChannels, STBI_rgb_alpha),
		stbi_load(file[2], &outImage.texWidth, &outImage.texHeight, &outImage.texChannels, STBI_rgb_alpha),
		stbi_load(file[3], &outImage.texWidth, &outImage.texHeight, &outImage.texChannels, STBI_rgb_alpha),
		stbi_load(file[4], &outImage.texWidth, &outImage.texHeight, &outImage.texChannels, STBI_rgb_alpha),
		stbi_load(file[5], &outImage.texWidth, &outImage.texHeight, &outImage.texChannels, STBI_rgb_alpha),
	};

	if (!pixels[0] || !pixels[1] || !pixels[2] || !pixels[3] || !pixels[4] || !pixels[5])
	{
		LOG_CORE_WARN("Failed to load texture file: " + *file[0]);
		LOG_CORE_WARN("Failed to load texture file: " + *file[1]);
		LOG_CORE_WARN("Failed to load texture file: " + *file[2]);
		LOG_CORE_WARN("Failed to load texture file: " + *file[3]);
		LOG_CORE_WARN("Failed to load texture file: " + *file[4]);
		LOG_CORE_WARN("Failed to load texture file: " + *file[5]);

		return;
	}

	outImage.m_desc = textureDesc;
	outImage.ptr[0] = pixels[0];
	outImage.ptr[1] = pixels[1];
	outImage.ptr[2] = pixels[2];
	outImage.ptr[3] = pixels[3];
	outImage.ptr[4] = pixels[4];
	outImage.ptr[5] = pixels[5];
	outImage.texSize = outImage.texWidth * outImage.texHeight * 4;


	//LOG_CORE_INFO(file);

	return;
}
