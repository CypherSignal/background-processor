#pragma once

#include <filesystem>
#include <EASTL/vector.h>

struct Color
{
	unsigned char r = 0;
	unsigned char g = 0;
	unsigned char b = 0;
	unsigned char a = 255;
};

struct Image
{
	int width, height, comp;
	eastl::vector<Color> imgData;
};

Image loadImage(const std::filesystem::path& filename);
void saveImage(const Image& img, const std::filesystem::path& file);

