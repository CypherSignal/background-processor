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
	unsigned int width, height;
};

struct HdmaRow
{
	unsigned char lineCounter; // should never be > 0x7f - "repeat" functionality in export doesn't exist (yet?).
	const unsigned char dummy = 0; // dummy byte that should be 0 - not used by hdma because it's delivered alongside cgramAddr
	unsigned char cgramAddr;
	unsigned char cgramData[2];
};

const unsigned int MaxWidth = 256;
const unsigned int MaxHeight = 224; // or otherwise, the # of scanlines the SNES outputs

struct PalettizedImage
{
	typedef eastl::fixed_vector<unsigned short, 256, false> PaletteTable;
	typedef eastl::fixed_vector<HdmaRow, MaxHeight, false> HdmaTable;
	PaletteTable palette;
	HdmaTable hdmaTable;
	eastl::vector<unsigned char> data;
	unsigned int width, height;
};