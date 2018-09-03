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
	int ogComp = 0;;
	Image img;
	unsigned char *data = stbi_load(filename.generic_string().c_str(), &ogWidth, &ogHeight, &ogComp, 3);
	if (data)
	{
		if (ogHeight % TileSize == 0 && ogWidth % TileSize == 0)
		{
			img.width = ogWidth;
			img.height = ogHeight;
			img.data.resize(img.width * img.height);
			memcpy(img.data.data(), data, img.width * img.height * 3);
		}
		else
		{
			img.width = (ogWidth + TileSize - 1) & ~(TileSize - 1);
			img.height = (ogHeight + TileSize - 1) & ~(TileSize - 1);
			img.data.resize(img.width * img.height);
			int row;
			for (row = 0; row < ogHeight; ++row)
			{
				memcpy(&img.data[row * img.width], &data[row * ogWidth * 3], ogWidth * 3);
			}
		}
	}
	stbi_image_free(data);
	return img;
}

template<typename T>
void writeToFile(T* data, size_t count, const std::filesystem::path& filePath)
{
	size_t bufferSize = sizeof(T) * count;
	void* buffer = new char[bufferSize];
	memcpy(buffer, data, bufferSize);
	Concurrency::create_task([buffer, bufferSize, filePath]()
	{
		FILE* out;
		if (!fopen_s(&out, filePath.generic_string().c_str(), "wb"))
		{
			fwrite(buffer, 1, bufferSize, out);
			fclose(out);
			delete buffer;
		}
	});
}

void saveImage(const Image& img, const std::filesystem::path& file)
{
	eastl::vector<unsigned char> buffer;
	buffer.reserve(img.data.size() * 4);

	auto writeFunc = [](void* context, void* data, int size) {
		auto buf = static_cast<eastl::vector<unsigned char>*>(context);
		auto oldSize = buf->size();
		buf->resize(oldSize + size);
		memcpy(buf->data() + oldSize, data, size);
	};

	stbi_write_png_to_func(writeFunc, &buffer, img.width, img.height, 3, img.data.data(),0);

	writeToFile(buffer.data(), buffer.size(), file);
}

void savePalettizedImage(const PalettizedImage& img, const std::filesystem::path& file)
{
	eastl::vector<unsigned char> buffer;
	buffer.reserve(img.data.size() * 2);

	auto writeFunc = [](void* context, void* data, int size) {
		auto buf = static_cast<eastl::vector<unsigned char>*>(context);
		auto oldSize = buf->size();
		buf->resize(oldSize + size);
		memcpy(buf->data() + oldSize, data, size);
	};

	stbi_write_png_to_func(writeFunc, &buffer, img.width, img.height, 1, img.data.data(),0);

	writeToFile(buffer.data(), buffer.size(), file);
}

void saveSnesPalette(eastl::fixed_vector<unsigned short, 256, false>& palette, const std::filesystem::path& file)
{
	writeToFile(palette.data(), palette.size(), file);
}

void saveSnesTiles(const PalettizedImage& img, const std::filesystem::path& file)
{
	struct Tile
	{
		unsigned char data[64];
	};
	const unsigned int MaxTiles = (256 / 8) * (224 / 8) + 1; // +1 to have an empty black tile
	eastl::fixed_vector<Tile, MaxTiles, false> snesTiles;
	
	unsigned int width = img.width;
	unsigned int height = img.height;
	snesTiles.resize(((width + 7) / 8) * ((height + 7) / 8));

	// transform the chars in img to an 8x8 tile, then resort the data as a bitplane
	for (unsigned int i = 0; i < height; ++i)
	{
		for (unsigned int j = 0; j < width; ++j)
		{
			snesTiles[(i / 8) * (width / 8) + j / 8].data[(i % 8) * 8 + j % 8] = img.data[i * width + j];
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

	writeToFile(snesTiles.data(), snesTiles.size(), file);
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

	writeToFile(snesTilemap.data(), snesTilemap.size(), file);
}
