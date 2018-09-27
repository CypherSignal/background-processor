#pragma once

#include "imageCommon.h"

struct ProcessImageParams
{
	// only acknowledged if lowBitDepthPalette is true - the maximum number of 16c palettes that will be generated
	int maxPalettes;
	// only acknowledged if lowBitDepthPalette is false - the maximum number of colors that will be generated across
	int maxColors;

	// the total number of hdmaChannels that will be utilized in the output
	int maxHdmaChannels;

	std::filesystem::path inFilePath;
	std::filesystem::path outDirPath;
};

Image getDepalettizedImage(const PalettizedImage& palettizedImg);
eastl::vector<unsigned short> getDepalettizedSnesImage(const PalettizedImage& palettizedImg);
Image getQuantizedImage(const Image& srcImg);

void processImage(const ProcessImageParams& params, ProcessImageStorage& out);

// utility for use when depalettizing the image based on hdma data
void updateHdmaAndPalette(const PalettizedImage::HdmaTable &hdmaTable, PalettizedImage::PaletteTable &activePalette, unsigned char &hdmaLineCounter, unsigned int &hdmaRowIdx);
