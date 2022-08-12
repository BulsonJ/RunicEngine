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
	};

	struct Texture
	{
		~Texture();

		TextureDesc desc;

		void* ptr = nullptr;
		int texWidth;
		int texHeight;
		int texChannels;
	};

	namespace TextureUtil
	{
		void LoadTextureFromFile(const char* file, TextureDesc textureDesc, Texture& outImage);
	}

}