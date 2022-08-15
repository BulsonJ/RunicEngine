#pragma once

namespace Runic
{
	struct TextureDesc
	{
		enum class Format
		{
			DEFAULT,
			NORMAL,
		} format;

		enum class Type
		{
			TEXTURE_2D,
			TEXTURE_CUBEMAP
		} type;
	};

	struct Texture
	{
		~Texture();

		TextureDesc m_desc;

		void* ptr = nullptr;
		int texWidth;
		int texHeight;
		int texChannels;
	};

	namespace TextureUtil
	{
		void LoadTextureFromFile(const char* file, TextureDesc textureDesc, Texture& outImage);
		void LoadCubemapFromFile(const char* file[6], TextureDesc textureDesc, Texture& outImage);
	}

}