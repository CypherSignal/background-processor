#pragma once

#include "imageCommon.h"

struct ProcessImageParams
{

	// if true, then the output image will be processed so that each tile addresses a 16c palette (i.e. 4bpp tilemap data)
	// if false, the output image will map a single pallette all on its own (i.e. 8bpp tilemap data)
	bool lowBitDepthPalette;
	bool generateHdmaData;

	// only acknowledged if lowBitDepthPalette is true - the maximum number of 16c palettes that will be generated
	int maxPalettes;
	// only acknowledged if lowBitDepthPalette is false - the maximum number of colors that will be generated across
	int maxColors;

	std::filesystem::path inFilePath;
	std::filesystem::path outDirPath;
};

struct ProcessImageStorage
{
	Image srcImg;
	PalettizedImage palettizedImg;
};

Image getDepalettizedImage(const PalettizedImage& palettizedImg);
eastl::vector<unsigned short> getDepalettizedSnesImage(const PalettizedImage& palettizedImg);

void processImage(const ProcessImageParams& params, ProcessImageStorage& out);

// utility for use when depalettizing the image based on hdma data
void updateHdmaAndPalette(const PalettizedImage::HdmaTable &hdmaTable, PalettizedImage::PaletteTable &activePalette, unsigned char &hdmaLineCounter, unsigned int &hdmaRowIdx);
