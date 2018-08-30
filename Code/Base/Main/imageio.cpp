#include "Pch.h"

#include "imageio.h"

#define STB_IMAGE_IMPLEMENTATION
#include <External/stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <External/stb/stb_image_write.h>

Image loadImage(const std::filesystem::path& filename)
{
	const int TileSize = 8;
	int ogWidth = 0;
	int ogHeight = 0;
	Image img;
	unsigned char *data = stbi_load(filename.generic_string().c_str(), &ogWidth, &ogHeight, &img.comp, 3);
	if (ogHeight % TileSize == 0 && ogWidth % TileSize == 0)
	{
		img.width = ogWidth;
		img.height = ogHeight;
		img.imgData.resize(img.width * img.height);
		img.comp = 3;
		memcpy(img.imgData.data(), data, img.width * img.height * 3);
	}
	else
	{
		img.width = (ogWidth + TileSize - 1) & ~(TileSize - 1);
		img.height = (ogHeight + TileSize - 1) & ~(TileSize - 1);
		img.imgData.resize(img.width * img.height);
		img.comp = 3;
		int row;
		for (row = 0; row < ogHeight; ++row)
		{
			memcpy(&img.imgData[row * img.width], &data[row * ogWidth * 3], ogWidth * 3);
		}
	}
	stbi_image_free(data);
	return img;
}

void saveImage(const Image& img, const std::filesystem::path& file)
{
	stbi_write_png(file.generic_string().c_str(), img.width, img.height, img.comp, img.imgData.data(), 0);
}