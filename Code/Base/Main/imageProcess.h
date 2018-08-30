#pragma once

#include "imageCommon.h"

struct ProcessImageParams
{
	// if true, then the output image will be processed so that each tile addresses a 16c palette (i.e. 4bpp tilemap data)
	// if false, the output image will map a single pallette all on its own (i.e. 8bpp tilemap data)
	bool lowBitDepthPalette;

	// only acknowledged if lowBitDepthPalette is true - the maximum number of 16c palettes that will be generated
	int maxPalettes;
	// only acknowledged if lowBitDepthPalette is false - the maximum number of colors that will be generated across
	int maxColors;

	// TODO: pointer to tile data, plt data, hdma data...
};

Image processImage(const Image& img, const ProcessImageParams& params);