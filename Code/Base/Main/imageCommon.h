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
	int width, height, comp;
	eastl::vector<Color> imgData;
};

struct PalettizedImage
{
	eastl::fixed_vector<Color, 256, false> palette;
	eastl::vector<unsigned char> img;
};