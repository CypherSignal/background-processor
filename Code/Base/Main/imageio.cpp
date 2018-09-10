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
	// TODO: having so many repeated open/closes in here is responsible for ~3ms of walltime per image
	// maybe look at optimizing this (e.g. writing out data to a single, shared, archive)
	FILE* out;
	if (!fopen_s(&out, filePath.generic_string().c_str(), "wb"))
	{
		fwrite(data, sizeof(T), count, out);
		fclose(out);
	}
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

void saveSnesPalette(const PalettizedImage::PaletteTable& palette, const std::filesystem::path& file)
{
	writeToFile(palette.data(), palette.size(), file);
}

void saveSnesTiles(const PalettizedImage& img, const std::filesystem::path& file)
{
	struct Tile
	{
		unsigned char data[64];
	};
	const unsigned int MaxTiles = (MaxWidth / 8) * (MaxHeight / 8) + 1; // +1 to have an empty black tile
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
	const unsigned int MaxTiles = (MaxWidth / 8) * (MaxHeight / 8);
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

void saveSnesHdmaTable(const PalettizedImage& img, const std::filesystem::path &file)
{
	struct HdmaRow
	{
		unsigned char lineCounter; // should never be > 0x7f - "repeat" functionality in export doesn't exist (yet?).
		const unsigned char dummy = 0; // dummy byte that should be 0 - not used by hdma because it's delivered alongside cgramAddr
		unsigned char cgramAddr;
		unsigned char cgramData[2];
	};

	for (unsigned int i = 0; i < img.hdmaTables.size(); ++i)
	{
		char fileSuffix[8];
		snprintf(fileSuffix, 8, "-%d", i);
		auto localPath = file;
		localPath.concat(fileSuffix);

		eastl::fixed_vector<HdmaRow, 224, false> hdmaOutput;
		const auto& hdmaActions = img.hdmaTables[i];
		for (const auto& hdmaAction : hdmaActions)
		{
			const auto& hdmaAction = hdmaActions[i];
			HdmaRow hdmaRow;
			hdmaRow.lineCounter = hdmaAction.lineCount;
			hdmaRow.cgramAddr = hdmaAction.paletteIdx;
			hdmaRow.cgramData[1] = (unsigned char)((hdmaAction.snesColor & 0x7f00) >> 8);
			hdmaRow.cgramData[0] = (unsigned char)((hdmaAction.snesColor & 0x00ff) >> 0);
			hdmaOutput.push_back(hdmaRow);
		}

		writeToFile(hdmaOutput.data(), hdmaOutput.size(), localPath);
	}
}