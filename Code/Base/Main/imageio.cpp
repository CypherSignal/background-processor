#include "Pch.h"

#include "imageio.h"

#define STB_IMAGE_IMPLEMENTATION
#include <External/stb/stb_image.h>

#define STBI_MSC_SECURE_CRT

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <External/stb/stb_image_write.h>

Image loadImage(const std::filesystem::path& filename)
{
	const int TileSize = 8;
	int ogWidth = 0;
	int ogHeight = 0;
	Image img;
	unsigned char *data = stbi_load(filename.generic_string().c_str(), &ogWidth, &ogHeight, &img.comp, 3);
	if (data)
	{
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
	}
	stbi_image_free(data);
	return img;
}

void saveImage(const Image& img, const std::filesystem::path& file)
{
	eastl::vector<unsigned char> buffer;
	buffer.reserve(img.imgData.size() * 4);

	auto writeFunc = [](void* context, void* data, int size) {
		auto buf = static_cast<eastl::vector<unsigned char>*>(context);
		auto oldSize = buf->size();
		buf->resize(oldSize + size);
		memcpy(buf->data() + oldSize, data, size);
	};

	stbi_write_png_to_func(writeFunc, &buffer, img.width, img.height, img.comp, img.imgData.data(),0);

	FILE* out;
	if (!fopen_s(&out, file.generic_string().c_str(), "wb"))
	{
		fwrite(buffer.data(), sizeof(unsigned char), buffer.size(), out);
		fclose(out);
	}
}

void savePalettizedImage(const PalettizedImage& pltImg, unsigned int width, unsigned int height, const std::filesystem::path& file)
{
	eastl::vector<unsigned char> buffer;
	buffer.reserve(pltImg.img.size() * 2);

	auto writeFunc = [](void* context, void* data, int size) {
		auto buf = static_cast<eastl::vector<unsigned char>*>(context);
		auto oldSize = buf->size();
		buf->resize(oldSize + size);
		memcpy(buf->data() + oldSize, data, size);
	};

	stbi_write_png_to_func(writeFunc, &buffer, width, height, 1, pltImg.img.data(),0);

	FILE* out;
	if (!fopen_s(&out, file.generic_string().c_str(), "wb"))
	{
		fwrite(buffer.data(), sizeof(unsigned char), buffer.size(), out);
		fclose(out);
	}
}

void saveSnesPalette(eastl::fixed_vector<Color, 256, false>& palette, const std::filesystem::path& file)
{
	eastl::fixed_vector<unsigned short, 256, false> snesPlt(palette.size());

	auto snesPltIter = snesPlt.begin();
	for (auto srcPltIter : palette)
	{
		unsigned short snesColor = ((srcPltIter.b & 0xf8) << 7) | ((srcPltIter.g & 0xf8) << 2) | ((srcPltIter.r & 0xf8) >> 3);
		(*snesPltIter) = snesColor;
		++snesPltIter;
	}
	
	FILE* out;
	if (!fopen_s(&out, file.generic_string().c_str(), "wb"))
	{
		fwrite(snesPlt.data(), sizeof(unsigned short), snesPlt.size(), out);
		fclose(out);
	}
}

void saveSnesTiles(eastl::vector<unsigned char>& img, unsigned int width, unsigned int height, const std::filesystem::path& file)
{
	struct Tile
	{
		unsigned char data[64];
	};
	const unsigned int MaxTiles = (256 / 8) * (224 / 8) + 1; // +1 to have an empty black tile
	eastl::fixed_vector<Tile, MaxTiles, false> snesTiles; // 897 
	snesTiles.resize(((width + 7) / 8) * ((height + 7) / 8));

	// transform the chars in img to an 8x8 tile, then resort the data as a bitplane
	for (unsigned int i = 0; i < height; ++i)
	{
		for (unsigned int j = 0; j < width; ++j)
		{
			snesTiles[(i / 8) * (width / 8) + j / 8].data[(i % 8) * 8 + j % 8] = img[i * width + j];
		}
	}

	Tile& emptyTile = snesTiles.push_back();
	memset(emptyTile.data, 0, sizeof(emptyTile.data));
	for (auto& snesTile : snesTiles)
	{
		Tile srcTile = snesTile;

		unsigned int tileIdx = 0;
		for (unsigned int bitplane = 0; bitplane < 8; bitplane += 2)
		{
			for (unsigned int k = 0; k < 8; ++k)
			{
				unsigned int mask = (1 << bitplane);
				snesTile.data[tileIdx] = (
					((srcTile.data[k * 8 + 0] & mask) << 7) |
					((srcTile.data[k * 8 + 1] & mask) << 6) |
					((srcTile.data[k * 8 + 2] & mask) << 5) |
					((srcTile.data[k * 8 + 3] & mask) << 4) |
					((srcTile.data[k * 8 + 4] & mask) << 3) |
					((srcTile.data[k * 8 + 5] & mask) << 2) |
					((srcTile.data[k * 8 + 6] & mask) << 1) |
					((srcTile.data[k * 8 + 7] & mask) << 0)
					) >> bitplane;

				tileIdx++;
				mask <<= 1;

				snesTile.data[tileIdx] = (
					((srcTile.data[k * 8 + 0] & mask) << 7) |
					((srcTile.data[k * 8 + 1] & mask) << 6) |
					((srcTile.data[k * 8 + 2] & mask) << 5) |
					((srcTile.data[k * 8 + 3] & mask) << 4) |
					((srcTile.data[k * 8 + 4] & mask) << 3) |
					((srcTile.data[k * 8 + 5] & mask) << 2) |
					((srcTile.data[k * 8 + 6] & mask) << 1) |
					((srcTile.data[k * 8 + 7] & mask) >> 0)
					) >> (bitplane + 1);
				tileIdx++;
			}
		}
	}

	FILE* out;
	if (!fopen_s(&out, file.generic_string().c_str(), "wb"))
	{
		fwrite(snesTiles.data(), sizeof(Tile), snesTiles.size(), out);
		fclose(out);
	}
}

void saveSnesTilemap(unsigned int width, unsigned int height, const std::filesystem::path& file)
{
	const unsigned int MaxTiles = (256 / 8) * (224 / 8);
	eastl::fixed_vector<unsigned short, MaxTiles, false> snesTilemap;
	snesTilemap.resize(MaxTiles, ((width + 7) / 8) * ((height + 7) / 8));
	
	unsigned short tileNumber = 0;
	for (unsigned int i = 0; i < height / 8; ++i)
	{
		for (unsigned int j = 0; j < width / 8; ++j)
		{
			snesTilemap[i * 32 + j] = tileNumber++;
		}
	}

	FILE* out;
	if (!fopen_s(&out, file.generic_string().c_str(), "wb"))
	{
		fwrite(snesTilemap.data(), sizeof(unsigned short), snesTilemap.size(), out);
		fclose(out);
	}
}
