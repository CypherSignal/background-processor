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
	unsigned char lineCount; // should never be > 0x7f - "repeat" functionality in export doesn't exist (yet?).
	unsigned char paletteIdx;
	unsigned short snesColor;
};

const unsigned int MaxWidth = 256;
const unsigned int MaxHeight = 224; // or otherwise, the # of scanlines the SNES outputs
const unsigned int MaxHdmaChannels = 8;
struct PalettizedImage
{
	typedef eastl::fixed_vector<unsigned short, 256, false> PaletteTable;
	typedef eastl::fixed_vector<HdmaRow, MaxHeight, false> HdmaTable;
	PaletteTable palette;
	eastl::fixed_vector<HdmaTable, MaxHdmaChannels, false> hdmaTables;
	eastl::vector<unsigned char> data;
	unsigned int width, height;
};