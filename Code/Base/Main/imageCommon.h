#pragma once

#include <EASTL/fixed_vector.h>
#include <EASTL/vector.h>

struct Color
{
	unsigned char r = 0;
	unsigned char g = 0;
	unsigned char b = 0;
};

struct Image
{
	eastl::vector<Color> data;
	int width, height;
};

struct PalettizedImage
{
	eastl::fixed_vector<unsigned short, 256, false> palette;
	eastl::vector<unsigned char> data;
	int width, height;
};